$content = Get-Content 'main.cpp' -Raw
# Fix the Bezier path - update path values and comment
$content = $content -replace '// Smooth B.*path to the Disney house entrance\..*?2\.6f, 32\);               // width, segments', @'
        // Smooth Bezier path to the Disney house entrance.
        // House is at z=-18; front wall at z~=-9.25; porch steps extend to z~=-8.
        drawBezierPath(
            { 0.0f, 0.f, 12.0f },   // p0 - camera spawn point
            { -1.5f, 0.f,  2.0f },  // p1 - drift left
            { 1.2f, 0.f, -5.0f },   // p2 - drift right
            { 0.0f, 0.f, -9.5f },   // p3 - arrives at porch steps
            3.0f, 36)               // width, segments
'@ -replace '(?s)', ''   # noop - just test

# Actually do it with proper multiline
$lines = Get-Content 'main.cpp'
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match 'p0 = player start area') {
        Write-Host "Found at line $($i+1): $($lines[$i])"
    }
    if ($lines[$i] -match 'drawBezierPath') {
        Write-Host "drawBezierPath at line $($i+1)"
    }
    if ($lines[$i] -match 'drawDisneyHouse') {
        Write-Host "drawDisneyHouse at line $($i+1)"
    }
}
