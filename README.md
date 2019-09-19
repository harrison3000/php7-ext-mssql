# php7-ext-mssql
This is just a port of the old mssql extension to newer PHP. 
**DON'T USE THIS FOR NEW APPS!**
It's only meant to help porting apps from PHP 5.6 to PHP 7
I only tested it on Ubuntu 19.04 64-bit and PHP7.2, YMMV

### How to compile
*  Clone the repo, obviously
*  Install the php-dev and freetds-dev packages
* *cd* into repository the directory
* *phpize*
* *./configure*
* *make*
* Add the extension to php.ini (or even better: a new file into conf.d)

### Changes 
* Persistent link functionality removed
* *mssql_close* do nothing now 
* Links are reference counted,
* Added to phpinfo the TDS Protocol version being used by the default link
* Added Warnings when values and column names are truncated when TDS Protocol version lower than 7.0
