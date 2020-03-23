SET SCRIPTPATH=%~dp0

mysql.exe -u root < "%SCRIPTPATH%db_setup.sql"
example_query_sync            example_user example_password
example_query_async_callbacks example_user example_password
example_metadata              example_user example_password
example_prepared_statements   example_user example_password