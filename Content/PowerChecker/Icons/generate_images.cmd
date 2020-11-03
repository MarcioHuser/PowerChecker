for %%i in (PowerChecker-Console.png) do (
	for %%s in (512 256) do (
		convert -verbose "%%~i" -resize %%sx%%s "%%~ni-%%s.png"
	)
)
