const express = require("express");
const path = require("path");
const app = express();

app.use(express.json());

app.use("/src", express.static(path.join(__dirname, "src")));

app.use(express.static(path.join(__dirname, "public")));

const PORT = process.env.PORT || 8080;
app.listen(PORT, () => console.log(`UI lista: http://localhost:${PORT}`));
