# Get version info from Git. example 1.2.3-45-g6789abc
$gitVersion = git describe --always --tags;
$gitDate = git log --pretty=format:"%H @ %ad" -n 1;
$gitBranch = git symbolic-ref HEAD;
Write-Output $gitVersion
Write-Output $gitDate

If ($gitBranch -eq "master") {
    $gitBranch = ""
} Else {
    $gitBranch = "-dev"
}

# Read template file, overwrite place holders with git version info
$newAssemblyContent = Get-Content $args[0] |
    %{$_ -replace '@VERSION@', ($gitVersion) } |
    %{$_ -replace '@DATE@', ($gitDate) };

# Write AssemblyInfo.cs file only if there are changes
If (-not (Test-Path $args[1]) -or ((Compare-Object (Get-Content $args[1]) $newAssemblyContent))) {
    $newAssemblyContent > $args[1];
}