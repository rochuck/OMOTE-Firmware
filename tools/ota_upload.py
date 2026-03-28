import subprocess
Import("env")

def ota_upload(source, target, env):
    firmware = str(source[0])
    host = env.GetProjectOption("upload_port", "OMOTE.local")
    url = f"http://{host}:3232/update"
    print(f"[OTA] Uploading {firmware} to {url}")
    result = subprocess.run([
        "curl", "--silent", "--show-error",
        "--write-out", "\n[OTA] HTTP %{http_code} in %{time_total}s\n",
        "--form", f"firmware=@{firmware}",
        "--max-time", "180",
        url,
    ])
    if result.returncode != 0:
        raise Exception(f"OTA upload failed (curl exit code {result.returncode})")

env.Replace(UPLOADCMD=ota_upload)
