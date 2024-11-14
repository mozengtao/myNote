- 帮助文档
	- [jq command](https://www.baeldung.com/linux/jq-command-json)

```bash
# json example file
{
  "cms": "123234",
  "docsis-mac": "4325324324"
}

jq -r keys repoinfo.json
# output
[
  "cms",
  "docsis-mac"
]

jq -r keys[] repoinfo.json
# output
cms
docsis-mac

jq -r ".cms" repoinfo.json
# output
123234

key=cms
jq -r ".[\"$key\"]" repoinfo.json
# output
123234
```