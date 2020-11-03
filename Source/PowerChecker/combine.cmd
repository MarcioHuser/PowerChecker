echo {"resources"^:{"icon"^:"PowerChecker-logo.png"}} > "%~dp0\..\..\mods\PowerChecker\resources.json"

jq --tab -s ".[0] * .[1]" "%~dp0\..\..\mods\PowerChecker\data.json" "%~dp0\..\..\mods\PowerChecker\resources.json" > "%~dp0\..\..\mods\PowerChecker\data-fixed.json"

copy /y "%~dp0\..\..\Content\PowerChecker\Icons\PowerChecker-Console-512.png" "%~dp0\..\..\mods\PowerChecker\PowerChecker-logo.png"

move /y "%~dp0\..\..\mods\PowerChecker\data-fixed.json" "%~dp0\..\..\mods\PowerChecker\data.json"

del /q "%~dp0\..\..\mods\PowerChecker\resources.json"
