$lines = Get-Content 'main.cpp'
$keep = $lines[0..1245] + $lines[1878..($lines.Count - 1)]
Set-Content 'main.cpp' $keep -Encoding UTF8
Write-Host "Done. Lines kept: $($keep.Count)"
