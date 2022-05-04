# QuicNIC

## Installing QuicNIC

Start a cloudlab experiment using the profile `QUIC-Server-2-Nodes` in the project `QUIC-Server`.
The profile source xml is (just for reference) as follows.

```shell
<rspec xmlns="http://www.geni.net/resources/rspec/3" xmlns:emulab="http://www.protogeni.net/resources/rspec/ext/emulab/1" xmlns:tour="http://www.protogeni.net/resources/rspec/ext/apt-tour/1" xmlns:jacks="http://www.protogeni.net/resources/rspec/ext/jacks/1" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.geni.net/resources/rspec/3    http://www.geni.net/resources/rspec/3/request.xsd" type="request">

<node xmlns="http://www.geni.net/resources/rspec/3" client_id="node-0">
<icon xmlns="http://www.protogeni.net/resources/rspec/ext/jacks/1" url="https://www.emulab.net/protogeni/jacks-stable/images/server.svg"/>
<sliver_type xmlns="http://www.geni.net/resources/rspec/3" name="raw-pc">
<disk_image xmlns="http://www.geni.net/resources/rspec/3" name="urn:publicid:IDN+utah.cloudlab.us+image+creditrpc-PG0//breakwater-xl170-2"/>
</sliver_type>
<hardware_type xmlns="http://www.geni.net/resources/rspec/3" name="xl170"/>
<services xmlns="http://www.geni.net/resources/rspec/3"/>
<interface xmlns="http://www.geni.net/resources/rspec/3" client_id="interface-0"/>
</node>

<node xmlns="http://www.geni.net/resources/rspec/3" client_id="node-1">
<icon xmlns="http://www.protogeni.net/resources/rspec/ext/jacks/1" url="https://www.emulab.net/protogeni/jacks-stable/images/server.svg"/>
<sliver_type xmlns="http://www.geni.net/resources/rspec/3" name="raw-pc">
<disk_image xmlns="http://www.geni.net/resources/rspec/3" name="urn:publicid:IDN+utah.cloudlab.us+image+creditrpc-PG0//breakwater-xl170-2"/>
</sliver_type>
<hardware_type xmlns="http://www.geni.net/resources/rspec/3" name="xl170"/>
<services xmlns="http://www.geni.net/resources/rspec/3"/>
<interface xmlns="http://www.geni.net/resources/rspec/3" client_id="interface-1"/>
</node>

<link xmlns="http://www.geni.net/resources/rspec/3" client_id="link-0">
<link_type xmlns="http://www.geni.net/resources/rspec/3" name="lan"/>
<interswitch xmlns="http://www.protogeni.net/resources/rspec/ext/emulab/1" allow="no"/>
<interface_ref xmlns="http://www.geni.net/resources/rspec/3" client_id="interface-0"/>
<interface_ref xmlns="http://www.geni.net/resources/rspec/3" client_id="interface-1"/>
<site xmlns="http://www.protogeni.net/resources/rspec/ext/jacks/1" id="undefined"/>  
<property xmlns="http://www.geni.net/resources/rspec/3" source_id="interface-0" dest_id="interface-1" capacity="10000000"/>
<property xmlns="http://www.geni.net/resources/rspec/3" source_id="interface-1" dest_id="interface-0" capacity="10000000"/>
</link>

<rspec_tour xmlns="http://www.protogeni.net/resources/rspec/ext/apt-tour/1">
<description xmlns="" type="markdown">Shenango-ready profile with xl170 cluster</description>
<instructions xmlns="" type="markdown">
For building shenango &amp; caladan. Based on https://www.cloudlab.us/p/CreditRPC/breakwater-compact/0.
The pre-configured disk image is `urn:publicid:IDN+utah.cloudlab.us+image+creditrpc-PG0:breakwater-xl170-2`.
</instructions>
</rspec_tour>

</rspec>
```

SSH to your cloudlab machine(s). For example:

```shell
ssh -o TCPKeepAlive=yes -o ServerAliveCountMax=20 -o ServerAliveInterval=15 saubhik@hp198.utah.cloudlab.us
ssh -o TCPKeepAlive=yes -o ServerAliveCountMax=20 -o ServerAliveInterval=15 saubhik@hp200.utah.cloudlab.us
```

cd to your NFS folder. For example:

```shell
cd /proj/quic-server-PG0/users/saubhik/tmp/
```

Clone `release-QuicNIC-v0.2` branch of mvfst:

```shell
git clone -b release-QuicNIC-v0.2 git@github.com:saubhik/mvfst.git
```

We will build binaries on the server machine, and just install linux dependencies to execute the binaries on the client
machine. On the server, execute:

```shell
sudo ./build_helper.sh
```

That's all for the server!

This creates a `_build` subdirectory in the mvfst root directory, with the following structure:

```
mvfst
|
|---_build
    |
    |---build
        |---
    |
    |---deps
        |---caladan
            |---
        |
        |---fizz
            |---
        |
        |---fmt
            |---
        |
        |---folly
            |---
        |
        |---googletest
            |---
        |
        |---zstd
            |---
    |
    |---iokerneld
```

`_build/build` is mvfst's build directory.
The dependencies of mvfst are built inside `deps/` directory under their respective subdirectories.
`iokerneld` is the IOKernel daemon binary inside `_build` dir.
You can modify caladan source and run `sudo ./build_helper.sh` on the server machine to re-build QuicNIC.
Subsequent runs re-builds only the binaries which depend on your modifications.

On the client, you only need to install dependencies to execute the binaries built by the server machine:

```shell
sudo apt-get update &&
sudo apt-get install -y \
g++ \
cmake \
m4 \
libboost-all-dev \
libevent-dev \
libdouble-conversion-dev \
libgoogle-glog-dev \
libgflags-dev \
libiberty-dev \
liblz4-dev \
liblzma-dev \
libsnappy-dev \
make \
zlib1g-dev \
binutils-dev \
libjemalloc-dev \
libssl-dev \
pkg-config \
libsodium-dev
```

That's all for the client!
You've installed QuicNIC!

### Running tperf

First, run the iokernel daemon on both server and client machine.
This has to be running in the background.
For example, inside tmux or another terminal pane or tab.

```shell
sudo ./_build/iokerneld
```

Then run the following `tperf` command on the server machine:

```shell
sudo ./_build/build/quic/tools/tperf/tperf \
-mode=server \
-host=10.10.1.1 \
-port=8000 \
-use_inplace_write=true \
-gso=true \
-max_batch_size=38 \
-congestion=none \
-block_size=33554432 \
-window=9000000 \
-duration=10 \
-log_loss=true \
-writes_per_loop=100000000
```

and run the following `tperf` command on the client machine:

```shell
sudo ./_build/build/quic/tools/tperf/tperf \
-mode=client \
-host=10.10.1.1 \
-port=8000 \
-use_inplace_write=true \
-gso=true \
-max_batch_size=38 \
-congestion=none \
-block_size=33554432 \
-window=9000000 \
-duration=10 \
-log_loss=true \
-writes_per_loop=100000000
```

---
