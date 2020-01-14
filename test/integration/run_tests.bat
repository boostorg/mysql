SET SCRIPTPATH=%~dp0

mysql.exe -u root < "%SCRIPTPATH%db_setup.sql"
mysql_integrationtests