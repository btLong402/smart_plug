import subprocess


def execute_mysql_script(host, user, password, database, sql_script_path):
    try:
        # Build the command to execute
        command = [
            'mysql',
            '-h', host,
            '-u', user,
            '-p' + password,
            database
        ]

        # Use subprocess to run the MySQL command with the script as input
        with open(sql_script_path, 'rb') as script:
            subprocess.run(command, stdin=script, check=True)

        print(f"MySQL script executed successfully.")
    except subprocess.CalledProcessError as e:
        print(f"Error executing MySQL script: {e}")
        
mysql_host = "localhost"
mysql_user = "root"
mysql_password = "123456"
mysql_database = "ftpdb"
mysql_script_path = "./database.sql"

execute_mysql_script(mysql_host, mysql_user, mysql_password, mysql_database, mysql_script_path)
