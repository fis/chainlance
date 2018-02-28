"""External dependencies for the chainlance project."""

def chainlance_repositories(
    omit_com_github_nanopb_nanopb=False,
    omit_com_google_protobuf=False,
    omit_six=False):
  """Imports dependencies for chainlance."""
  if not omit_com_github_nanopb_nanopb:
    com_github_nanopb_nanopb()
  if not omit_com_google_protobuf:
    com_google_protobuf()
  if not omit_six:
    six()

# nanopb (master @ 1c987eb09, https://github.com/nanopb/nanopb/commit/1c987eb09)

def com_github_nanopb_nanopb():
  native.new_http_archive(
      name = "com_github_nanopb_nanopb",
      urls = ["https://github.com/nanopb/nanopb/archive/1c987eb09d70f80a2752ede407e0f4060375e8de.zip"],
      strip_prefix = "nanopb-1c987eb09d70f80a2752ede407e0f4060375e8de",
      sha256 = "9742ad36a41e878b7cf80048b58470f3e7650852bf6c2f338b497dbb702ef121",
      build_file = "@fi_zem_chainlance//tools:nanopb.BUILD",
  )

# protobuf (master @ ae55fd2cc, https://github.com/google/protobuf/commit/ae55fd2cc)

def com_google_protobuf():
  native.http_archive(
      name = "com_google_protobuf",
      urls = ["https://github.com/google/protobuf/archive/ae55fd2cc52849004de21a7e26aed7bfe393eaed.zip"],
      strip_prefix = "protobuf-ae55fd2cc52849004de21a7e26aed7bfe393eaed",
      sha256 = "80e30ede3cdb3170f10e8ad572a0db934b5129a8d956f949ed80e430c62f8e00",
  )

# six (1.10.0), needed by @com_google_protobuf//:protobuf_python

def six():
  native.new_http_archive(
      name = "six_archive",
      build_file = "@com_google_protobuf//:six.BUILD",
      sha256 = "105f8d68616f8248e24bf0e9372ef04d3cc10104f1980f54d57b2ce73a5ad56a",
      url = "https://pypi.python.org/packages/source/s/six/six-1.10.0.tar.gz#md5=34eed507548117b2ab523ab14b2f8b55",
  )
  native.bind(
      name = "six",
      actual = "@six_archive//:six",
  )
