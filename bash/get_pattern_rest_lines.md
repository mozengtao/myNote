
## 获取匹配行及之后所有内容
sed -n '/pattern/,$p' file.txt

## 获取匹配行之后所有内容（不包括匹配行）
awk '/pattern/{found=1;next} found' file.txt

## 有分隔符: 返回分隔符之后的所有行，无分隔符: 返回所有行

- test script
```bash
awk '
  BEGIN { buffer = ""; found = 0 }
  /---/ && !found { buffer = ""; found = 1; next }
  !found { buffer = buffer $0 "\n"; next }
  found { print }
  END { if (!found) printf "%s", buffer }
' data.txt
```

- test data 1
```
Header 1
Header 2
---
Data 1
Data 2
```

- output
```
Data 1
Data 2
```

- test data 2
```
Header 1
Header 2
---
Data 1
Data 2
```

- output
```
Data 1
Data 2
Data 3
```