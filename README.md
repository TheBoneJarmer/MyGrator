# MyGrator
## About
MyGrator is a basic command line tool for running migrations in a MySQL database. The tool is very basic in the sense that it merely executes SQL scripts in a folder and keeps track of which migrations it has run.

## Usage
> **Note!** MyGrator only works for Linux currently

MyGrator expects a folder with SQL scripts as its migration folder. The filenames will be sorted **alphabetically** before the contents will be executed. Therefore I highly recommend prefixing your filenames.

### Command
```
mygrator <db_host> <db_user> <db_pass> <db_schema>
```

## Notes
* Unlike many other migration tools, MyGrator does not have a rollback function. If you want to revert a change, simply add a new migration. Do **not** just start modifying the database. This could intervene with the migration history.
* The default history table is called `__migrations` by but can be changed with the argument `-t`.
* When a SQL error occurrs the tool will stop running and return a non-zero exit code.

## Contributions
Contributions are welcome. I use the GitFlow branching model so use the **develop** branch please as target branch. Pull requests targeting any other branch will be discarded. Thanks in advance!
