@echo off
echo Starting test...
"C:\Program Files\Meta Quest Developer Hub\resources\bin\adb.exe" shell pm path com.android.babylonnative.playground > path_output.txt
echo Path command completed
type path_output.txt
echo Test completed