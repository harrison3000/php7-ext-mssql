# php7-ext-mssql

This is just a port of the old mssql extension to newer PHP.

**DON'T USE THIS FOR NEW APPS!**

It's only meant to help porting apps from PHP <= 5.6 to PHP 8

I only tested it on Ubuntu 64-bit and PHP 8, YMMV

### How to compile

* Clone the repo, obviously
* Install the php-dev and freetds-dev packages
* `cd` into repository the directory
* `phpize`
* `./configure`
* `make`
* Add a new `.ini` file into `/etc/php/???/conf.d` (the actual path will vary) enabling the extension

### Changes to old version

* Persistent link functionality removed
* `mssql_close` is now a NOP, connections are automatically closed when the last reference to the link resource is destroyed (either by going out of scope, being garbage collected or having the link variable overwritten with something else, just like objects)
* Added to phpinfo() the TDS Protocol version being used by the default link
* Added Warnings when VARCHAR values and/or column names are truncated (when using a TDS protocol version lower than 7.0)
