$pathsToCheck = @(
    'C:\Program Files\Docker\Docker\resources\bin',
    "$env:LOCALAPPDATA\Programs\Docker\Docker\resources\bin",
    "$env:USERPROFILE\AppData\Local\Programs\Docker\Docker\resources\bin"
)
$found = $null
foreach ($p in $pathsToCheck) {
    if (Test-Path (Join-Path $p 'docker-credential-desktop.exe')) {
        $found = $p
        break
    }
}
if (-not $found) {
    Write-Output 'NOTFOUND'
    exit 1
}
$userPath = [Environment]::GetEnvironmentVariable('PATH','User')
if (-not $userPath) {
    setx PATH $found | Out-Null
} elseif ($userPath -notlike "*$found*") {
    setx PATH "$userPath;$found" | Out-Null
}
# update current session PATH for immediate use
$env:PATH = $env:PATH + ';' + $found
Write-Output "ADDED:$found"
