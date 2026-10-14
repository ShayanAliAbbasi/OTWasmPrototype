// @otlicense
// File: run-web.cjs
// @otlicense-end

const fs = require('fs');
const http = require('http');
const path = require('path');
const { chromium } = require('playwright');
const playwrightVersion = require('playwright/package.json').version;

const [sessionDir, runsText, warmupRunsText, entitiesText, warmupFramesText, framesText, operationsText, plotIterationsText] = process.argv.slice(2);
if (!plotIterationsText) {
    throw new Error('Usage: run-web.cjs SESSION_DIR RUNS WARMUP_RUNS ENTITIES WARMUP_FRAMES FRAMES OPERATIONS PLOT_ITERATIONS');
}

const prototypeDir = path.resolve(__dirname, '..');
const buildDir = process.env.OPENTWIN_WASM_BUILD_DIR || path.join(prototypeDir, 'build', 'wasm');
const entrypoint = path.join(buildDir, 'OpenTwinPrototype.html');
const chromePath = process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const headless = process.env.BENCHMARK_HEADLESS === '1';
const timeoutMs = Number.parseInt(process.env.BENCHMARK_TIMEOUT_MS || '180000', 10);
const runs = Number.parseInt(runsText, 10);
const warmupRuns = Number.parseInt(warmupRunsText, 10);

if (!fs.existsSync(entrypoint)) {
    throw new Error(`WebAssembly entrypoint not found: ${entrypoint}`);
}
if (!fs.existsSync(chromePath)) {
    throw new Error(`Chrome executable not found: ${chromePath}. Set CHROME_PATH to a Chromium-based browser.`);
}

const mimeTypes = new Map([
    ['.html', 'text/html; charset=utf-8'],
    ['.js', 'text/javascript; charset=utf-8'],
    ['.wasm', 'application/wasm'],
    ['.svg', 'image/svg+xml'],
    ['.json', 'application/json']
]);

function createServer() {
    return http.createServer((request, response) => {
        const requestUrl = new URL(request.url, 'http://127.0.0.1');
        if (requestUrl.pathname === '/favicon.ico') {
            response.writeHead(204).end();
            return;
        }
        const relativePath = decodeURIComponent(requestUrl.pathname).replace(/^\/+/, '');
        const filePath = path.resolve(buildDir, relativePath || 'OpenTwinPrototype.html');
        if (!filePath.startsWith(`${path.resolve(buildDir)}${path.sep}`) && filePath !== path.resolve(buildDir)) {
            response.writeHead(403).end('Forbidden');
            return;
        }

        fs.readFile(filePath, (error, content) => {
            if (error) {
                response.writeHead(error.code === 'ENOENT' ? 404 : 500).end(error.message);
                return;
            }
            response.writeHead(200, {
                'Content-Type': mimeTypes.get(path.extname(filePath)) || 'application/octet-stream',
                'Cache-Control': 'no-store'
            });
            response.end(content);
        });
    });
}

function padded(index) {
    return String(index).padStart(3, '0');
}

async function runPage(browser, baseUrl, kind, index) {
    const id = `web-${kind}-${padded(index)}`;
    const outputPath = path.join(sessionDir, 'web', `${kind}-run-${padded(index)}.json`);
    const logPath = path.join(sessionDir, 'logs', 'web', `${kind}-run-${padded(index)}.log`);
    const context = await browser.newContext({
        viewport: { width: 1440, height: 900 },
        deviceScaleFactor: 1,
        serviceWorkers: 'block'
    });
    const page = await context.newPage();
    const logs = [];
    let resolveResult;
    let rejectResult;
    const resultPromise = new Promise((resolve, reject) => {
        resolveResult = resolve;
        rejectResult = reject;
    });

    page.on('console', message => {
        const text = message.text();
        logs.push(`[${message.type()}] ${text}`);
        if (text.startsWith('BENCHMARK_RESULT ')) {
            try {
                resolveResult(JSON.parse(text.slice('BENCHMARK_RESULT '.length)));
            }
            catch (error) {
                rejectResult(error);
            }
        }
    });
    page.on('pageerror', error => logs.push(`[pageerror] ${error.stack || error.message}`));

    const query = new URLSearchParams({
        benchmark: '1',
        entities: entitiesText,
        warmupFrames: warmupFramesText,
        frames: framesText,
        operations: operationsText,
        plotIterations: plotIterationsText,
        cacheBust: `${Date.now()}-${id}`
    });
    const started = process.hrtime.bigint();
    await page.goto(`${baseUrl}/OpenTwinPrototype.html?${query}`, {
        waitUntil: 'domcontentloaded',
        timeout: timeoutMs
    });

    let timeoutHandle;
    const timeout = new Promise((_, reject) => {
        timeoutHandle = setTimeout(() => reject(new Error(`Timed out waiting for ${id}`)), timeoutMs);
    });
    const result = await Promise.race([resultPromise, timeout]);
    clearTimeout(timeoutHandle);
    if (result.metrics?.complex_model_loaded !== true) {
        throw new Error(`${id} did not load the compressed OSGB benchmark model`);
    }
    const navigationToResultMs = Number(process.hrtime.bigint() - started) / 1e6;
    const browserMetrics = await page.evaluate(() => {
        const navigation = performance.getEntriesByType('navigation')[0];
        const wasmResource = performance.getEntriesByType('resource').find(entry => entry.name.endsWith('.wasm'));
        const memory = performance.memory;
        return {
            user_agent: navigator.userAgent,
            hardware_concurrency: navigator.hardwareConcurrency,
            navigation_duration_ms: navigation ? navigation.duration : null,
            wasm_transfer_bytes: wasmResource ? wasmResource.transferSize : null,
            wasm_encoded_bytes: wasmResource ? wasmResource.encodedBodySize : null,
            wasm_decoded_bytes: wasmResource ? wasmResource.decodedBodySize : null,
            js_heap_used_bytes: memory ? memory.usedJSHeapSize : null,
            js_heap_total_bytes: memory ? memory.totalJSHeapSize : null
        };
    });

    result.run_id = id;
    result.harness = {
        classification: kind,
        run_index: index,
        browser_navigation_to_result_ms: navigationToResultMs,
        browser: browserMetrics,
        node_version: process.version,
        playwright_version: playwrightVersion,
        headless
    };
    fs.writeFileSync(outputPath, `${JSON.stringify(result, null, 2)}\n`);
    fs.writeFileSync(logPath, `${logs.join('\n')}\n`);
    await context.close();
    process.stdout.write(`Web ${kind} run ${padded(index)} complete\n`);
}

async function main() {
    fs.mkdirSync(path.join(sessionDir, 'web'), { recursive: true });
    fs.mkdirSync(path.join(sessionDir, 'logs', 'web'), { recursive: true });

    const server = createServer();
    await new Promise((resolve, reject) => {
        server.once('error', reject);
        server.listen(0, '127.0.0.1', resolve);
    });
    const address = server.address();
    const baseUrl = `http://127.0.0.1:${address.port}`;

    const browser = await chromium.launch({
        executablePath: chromePath,
        headless,
        args: [
            '--disable-background-timer-throttling',
            '--disable-backgrounding-occluded-windows',
            '--disable-renderer-backgrounding'
        ]
    });

    try {
        for (let i = 1; i <= warmupRuns; ++i) {
            await runPage(browser, baseUrl, 'warmup', i);
        }
        for (let i = 1; i <= runs; ++i) {
            await runPage(browser, baseUrl, 'measured', i);
        }
    }
    finally {
        await browser.close();
        await new Promise(resolve => server.close(resolve));
    }
}

main().catch(error => {
    process.stderr.write(`${error.stack || error.message}\n`);
    process.exitCode = 1;
});
