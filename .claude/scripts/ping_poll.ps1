# UE5AIAssistant HTTP 서버가 뜰 때까지 ping 재시도.
# 사용: powershell -NoProfile -ExecutionPolicy Bypass -File ping_poll.ps1
# 에디터 로딩(첫 셰이더 컴파일 포함)이 끝나면 PING_OK + engineVersion 출력.
param(
    [int]$Retries = 30,
    [int]$DelaySec = 5,
    [string]$Uri = 'http://localhost:58080/api/ping'
)
$ok = $false
for ($i = 0; $i -lt $Retries; $i++) {
    try {
        $r = Invoke-RestMethod -Uri $Uri -TimeoutSec 3 -ErrorAction Stop
        Write-Output ('PING_OK: ' + ($r | ConvertTo-Json -Compress))
        $ok = $true
        break
    } catch {
        Start-Sleep -Seconds $DelaySec
    }
}
if (-not $ok) { Write-Output 'PING_FAIL: HTTP server not up within timeout' }
