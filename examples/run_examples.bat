SET SCRIPTPATH=%~dp0

mysql.exe -u root < "%SCRIPTPATH%db_setup.sql"
example_query_sync  example_user example_password
example_query_async example_user example_password
example_metadata    example_user example_password