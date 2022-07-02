#!/bin/bash
# apply markdown file content quarks


# rewrite br tags
sed -i 's|<br>|<br/>|g' $@ 
