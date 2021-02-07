{
  "targets": [
    {
      "include_dirs" : [
        "<!(node -e \"require('nan')\")",
      ],
      "target_name": "addon",
      "sources": [ "native/lib.cc"],
      "defines": [ "_UNICODE", "UNICODE" ]
    }
  ]
}