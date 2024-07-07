REM Copyright 2024 Amit Kumar Mehar. All Rights Reserved.

cmake . -B Intermediate

cmake --build Intermediate --config Debug
cmake --build Intermediate --config Release