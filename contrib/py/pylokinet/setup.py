from setuptools import setup, find_packages



setup(
  name="pylokinet",
  version="0.0.1",
  license="ZLIB",
  author="jeff",
  author_email="jeff@i2p.rocks",
  description="lokinet python bindings",
  url="https://github.com/loki-project/loki-network",
  install_requires=["pysodium", "requests", "python-dateutil"],
  packages=find_packages())