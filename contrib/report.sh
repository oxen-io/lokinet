#!/bin/bash

# generates a weekly report template
# uses github cli https://github.com/cli/cli

since=$(date +%Y-%m-%dT%H:%M:%SZ --date='last week')
gh pr list -R oxen-io/loki-network -s merged --json mergedAt,title,url -q "map(select(.mergedAt >= \"$since\")) | map(.title + \" \" + .url | @text) | join(\"\n\n\")"
