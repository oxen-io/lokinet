# lokinet-bootserv

cgi executable for serving a random RC for bootstrap from a nodedb

## configuring

copy the example config (privileged)

    # cp configs/lokinet-bootserv.ini /usr/local/etc/lokinet-bootserv.ini
    
edit config to have proper values, 
specifically  make sure the `[nodedb]` section has a `dir` value that points to a static copy of a healthy nodedb

## building

to build:

    $ make

## installing (priviledged)

install cgi binary:

    # cp lokinet-bootserv /usr/local/bin/lokinet-bootserv 

set up with nginx cgi:

    # cp configs/lokinet-bootserv-nginx.conf /etc/nginx/sites-available/lokinet-bootserv.conf
    # ln -s /etc/nginx/sites-available/lokinet-bootserv.conf /etc/nginx/sites-enabled/ 

## maintainence

add the following to crontab

    0 0 * * * /usr/local/bin/lokinet-bootserv --cron
