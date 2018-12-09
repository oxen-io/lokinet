class LokiNetwork < Formula
  desc "Lokinet is a private, decentralized and Market based Sybil resistant overlay network for the internet"
  homepage "https://loki.network"
  url "https://github.com/loki-project/loki-network/archive/v0.3.1.tar.gz"
  sha256 "a745cd156fd08be575f0e9f2fd56f4d6cde896363103c6f49499c90451021b34"
  
  depends_on "cmake" => :build
  depends_on "rapidjson" => :recommended
  depends_on "abyss" => :optional
  depends_on "ninja" => :build
  option "with-jsonrpc" "Build with JSON-RPC support"
  option "without-avx2" "Build with AVX2 support"
  option "with-netns" "Build with network namespace support"

  def install
    ENV["USING_CLANG"] = "ON"

    # build time options
    ENV["USE_LIBABYSS"] = "ON" if build.with? "abyss"
    ENV["USE_AVX2"] = "ON" if build.with? "avx2"
    ENV["USE_NETNS"] = "ON" if build.with? "netns"

    system "mkdir", "-p", "#{buildpath}/build"
    system "cd", "#{buildpath}/build"

    cmake_args = *std_cmake_args
    cmake_args << "-GNinja"

    system "cmake", "-B", "#{buildpath}/build", "-S", "#{buildpath}", *cmake_args
    system "ninja", "-C", "#{buildpath}/build"
    system "ninja", "-C", "#{buildpath}/build", "install"

    bin.install "#{buildpath}/build/lokinet"
    bin.install "#{buildpath}/lokinet-bootstrap"
    bin.install "#{buildpath}/build/dns"
    bin.install "#{buildpath}/build/llarpc"
    bin.install "#{buildpath}/build/rcutil"
  end

  test do
    system "#{buildpath}/build/testAll"
  end
end
