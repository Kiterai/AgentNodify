const fs = require("node:fs");

const data = JSON.parse(fs.readFileSync(0, "utf8"));
data.javascript = "Node.jsからの出力";
process.stdout.write(JSON.stringify(data) + "\n");
