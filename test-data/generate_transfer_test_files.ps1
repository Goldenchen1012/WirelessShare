param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'transfer-files')
)

$sizes = [ordered]@{
    'test-1.5KB.bin' = 1536
    'test-8KB.bin' = 8192
    'test-16KB.bin' = 16384
    'test-30KB.bin' = 30720
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

foreach ($entry in $sizes.GetEnumerator()) {
    $bytes = New-Object byte[] $entry.Value
    for ($index = 0; $index -lt $bytes.Length; ++$index) {
        $bytes[$index] = $index % 251
    }
    [System.IO.File]::WriteAllBytes((Join-Path $OutputDirectory $entry.Key), $bytes)
}

$textLength = 30 * 1024
$line = [System.Text.Encoding]::UTF8.GetBytes(
    "WirelessShare 30 KB text transfer test. 0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ`r`n"
)
$textBytes = New-Object byte[] $textLength
for ($offset = 0; $offset -lt $textBytes.Length; $offset += $line.Length) {
    $count = [Math]::Min($line.Length, $textBytes.Length - $offset)
    [Array]::Copy($line, 0, $textBytes, $offset, $count)
}
[System.IO.File]::WriteAllBytes((Join-Path $OutputDirectory 'test-30KB-text.txt'), $textBytes)

Get-ChildItem -LiteralPath $OutputDirectory -File |
    Sort-Object Length, Name |
    Select-Object Name, Length, @{Name = 'SHA256'; Expression = {
        (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
    }}
