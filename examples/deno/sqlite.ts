import { DatabaseSync } from "node:sqlite";
const DB_PATH = Deno.args[0] ?? "deno.sqlite";

{
  console.log("create database");
  const db = new DatabaseSync(DB_PATH);
  db.exec(`CREATE TABLE IF NOT EXISTS requests (url TEXT)`);
  db.close();
}

Deno.serve((req) => {
  console.log(`request: ${req.url}`);
  const db = new DatabaseSync(DB_PATH);
  const stmt = db.prepare(`INSERT INTO requests (url) VALUES (?)`);
  const res = stmt.run(req.url);
  db.close();
  return Response.json(res);
});
