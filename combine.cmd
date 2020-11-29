pushd "%~dp0\Mods\PowerChecker"

copy /y ..\..\Content\PowerChecker\Icons\powercheck.png powercheck.png

echo {"resources"^:{"icon"^:"powercheck.png"}} > "resources.json"

jq --tab -s ".[0] * .[1]" "data.json" "resources.json" > "data-fixed.json"

move /y "data-fixed.json" "data.json"

del /q "resources.json"

popd
