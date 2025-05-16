vcl 4.1;
backend default {
  //.host = "backend";
  //.port = "8000";
  .path = "/sock/backend";
}
sub vcl_recv {
  return (pass);
}
sub vcl_backend_fetch {
  set bereq.http.connection = "close";
  return (fetch);
}
sub vcl_backend_response {
    set beresp.uncacheable = true;
}
