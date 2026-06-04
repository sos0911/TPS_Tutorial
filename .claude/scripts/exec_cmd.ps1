# UE5AIAssistant 명령 실행 헬퍼.
# 사용: powershell -NoProfile -ExecutionPolicy Bypass -File exec_cmd.ps1 -BodyFile <body.json>
# body.json 형식: {"command":"<cmd>","args":{...}}
# (curl/WebFetch가 막힌 환경 대비 PowerShell 사용. 쉘 이스케이프로 JSON이 깨지므로 본문은 파일로 전달.)
param(
    [Parameter(Mandatory = $true)][string]$BodyFile,
    [string]$Uri = 'http://localhost:58080/api/execute'
)
$body = Get-Content -Raw -Path $BodyFile
try {
    $r = Invoke-RestMethod -Uri $Uri -Method Post -ContentType 'application/json' -Body $body -TimeoutSec 60 -ErrorAction Stop
    $r | ConvertTo-Json -Depth 40
} catch {
    Write-Output ('EXEC_ERR: ' + $_.Exception.Message)
}
