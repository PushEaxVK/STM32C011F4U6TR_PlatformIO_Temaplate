const https = require("https");
const http = require("http");
const fs = require("fs");
const path = require("path");
const { execSync } = require("child_process");
const { URL } = require("url"); // <-- важливо для старих Node

const VERSION = "0.12.0-7";
const ZIP_NAME = `xpack-openocd-${VERSION}-win32-x64.zip`;
const START_URL = `https://github.com/xpack-dev-tools/openocd-xpack/releases/download/v${VERSION}/${ZIP_NAME}`;

const PROJECT_ROOT = process.cwd();
const PACKAGES_DIR = path.join(PROJECT_ROOT, "packages");
const DOWNLOAD_PATH = path.join(PROJECT_ROOT, ZIP_NAME);
const EXTRACT_DIR = path.join(PACKAGES_DIR, `xpack-openocd-${VERSION}`);

function getClient(urlStr) {
  const u = new URL(urlStr);
  return u.protocol === "https:" ? https : http;
}

function downloadFollowRedirects(urlStr, destPath, maxRedirects = 12) {
  return new Promise((resolve, reject) => {
    const out = fs.createWriteStream(destPath);

    const doRequest = (currentUrl, redirectsLeft) => {
      const client = getClient(currentUrl);

      const req = client.get(
        currentUrl,
        { headers: { "User-Agent": "node" } },
        (res) => {
          // Редиректи
          if ([301, 302, 303, 307, 308].includes(res.statusCode)) {
            const loc = res.headers.location;
            res.resume();

            if (!loc) {
              out.close(() => fs.unlink(destPath, () => {}));
              return reject(new Error(`Redirect ${res.statusCode} without Location header`));
            }
            if (redirectsLeft <= 0) {
              out.close(() => fs.unlink(destPath, () => {}));
              return reject(new Error("Too many redirects"));
            }

            const nextUrl = new URL(loc, currentUrl).toString();
            return doRequest(nextUrl, redirectsLeft - 1);
          }

          if (res.statusCode !== 200) {
            res.resume();
            out.close(() => fs.unlink(destPath, () => {}));
            return reject(new Error(`Download failed: HTTP ${res.statusCode}`));
          }

          res.pipe(out);
          out.on("finish", () => out.close(resolve));
        }
      );

      req.on("error", (err) => {
        out.close(() => fs.unlink(destPath, () => {}));
        reject(err);
      });
    };

    doRequest(urlStr, maxRedirects);
  });
}

async function main() {
  console.log("Creating packages directory...");
  fs.mkdirSync(PACKAGES_DIR, { recursive: true });

  // Якщо вже встановлено — нічого не робимо
  if (fs.existsSync(path.join(EXTRACT_DIR, "bin", "openocd.exe"))) {
    console.log("Already installed:", EXTRACT_DIR);
    return;
  }

  console.log("Downloading OpenOCD...");
  await downloadFollowRedirects(START_URL, DOWNLOAD_PATH);
  console.log("Downloaded:", DOWNLOAD_PATH);

  console.log("Extracting archive...");
  execSync(
    `powershell -NoProfile -Command "Expand-Archive -Force '${DOWNLOAD_PATH}' '${PACKAGES_DIR}'"`,
    { stdio: "inherit" }
  );

  if (!fs.existsSync(EXTRACT_DIR)) {
    throw new Error(`Extraction failed. Expected folder: ${EXTRACT_DIR}`);
  }

  console.log("Creating package.json...");
  const manifest = {
    name: "tool-openocd",
    version: VERSION,
    description: "xPack OpenOCD local wrapper for PlatformIO",
    keywords: ["openocd", "debug", "stm32", "stlink"],
    system: ["windows_amd64"],
  };
  fs.writeFileSync(
    path.join(EXTRACT_DIR, "package.json"),
    JSON.stringify(manifest, null, 2),
    "utf8"
  );

  console.log("Cleaning up zip...");
  try { fs.unlinkSync(DOWNLOAD_PATH); } catch {}

  const openocdExe = path.join(EXTRACT_DIR, "bin", "openocd.exe");
  const scriptsDir = path.join(EXTRACT_DIR, "openocd", "scripts");

  console.log("\nDONE.");
  console.log("OpenOCD:", openocdExe);
  console.log("Scripts:", scriptsDir);

  console.log("\nplatformio.ini вариант (може спрацювати):");
  console.log(`platform_packages =\n    tool-openocd@file:///${EXTRACT_DIR.replace(/\\/g, "/")}`);

  console.log("\nНайнадійніше (через custom + -s scripts):");
  console.log(
`upload_protocol = custom
upload_command =
  ${openocdExe}
  -s ${scriptsDir}
  -f interface/stlink.cfg
  -f target/stm32c0x.cfg
  -c "program $SOURCE verify reset exit"`
  );
}

main().catch((e) => {
  console.error("Error:", e && e.message ? e.message : e);
  process.exit(1);
});
