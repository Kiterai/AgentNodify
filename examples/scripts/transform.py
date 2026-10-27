import json
import sys

data = json.load(sys.stdin)
data["python"] = "Pythonからの出力"
print(json.dumps(data, ensure_ascii=False))
