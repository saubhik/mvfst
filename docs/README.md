# Setting up mvfst on shenango (on cloudlab)

DISCLAIMER: I executed these instructions on a fresh cloudlab experiment on 2021-11-24.

## Cloudlab

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

SSH to your cloudlab machine(s). For example, I use the following:
```shell
ssh -o TCPKeepAlive=yes -o ServerAliveCountMax=20 -o ServerAliveInterval=15 saubhik@hp198.utah.cloudlab.us
ssh -o TCPKeepAlive=yes -o ServerAliveCountMax=20 -o ServerAliveInterval=15 saubhik@hp200.utah.cloudlab.us
```

cd to your NFS folder. I use the following:
```shell
cd /proj/quic-server-PG0/users/saubhik/tmp/
```

## Client node
Choose one node for server, and the other one for client.
NOTE: You need to build and install the libraries only in the server machine.

For the client machine, install the following:
```shell
sudo apt-get update && \
sudo apt-get install \
    g++ \
    cmake \
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

And then build shenango and run the iokernel as explained in the coming sections.
You also need to install g++ 11.1.0 on the client node execute mvfst and folly binaries.
Follow the next section on installing g++ 11.1.0.
You don't need to install anything more on the client node.

### Install gcc 11.1.0

folly & shenango's bindings/cc requires g++-11.
Unfortunately, it doesn't come by default in the ubuntu version we are using.
So, we need to do the following.

```shell
sudo apt install build-essential manpages-dev software-properties-common && \
sudo add-apt-repository ppa:ubuntu-toolchain-r/test && \
sudo apt update && sudo apt install gcc-11 g++-11
```

Then, we need to be able to switch the default gcc to 11
```shell
sudo update-alternatives --remove-all cpp && \
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 70 --slave /usr/bin/g++ g++ /usr/bin/g++-7 --slave /usr/bin/gcov gcov /usr/bin/gcov-7 --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-7 --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-7  --slave /usr/bin/cpp cpp /usr/bin/cpp-7 && \
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 110 --slave /usr/bin/g++ g++ /usr/bin/g++-11 --slave /usr/bin/gcov gcov /usr/bin/gcov-11 --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-11 --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-11  --slave /usr/bin/cpp cpp /usr/bin/cpp-11
```

Note that you might not need to do `sudo update-alternatives --remove-all cpp` before the last 2 commands.
If there's any error, execute just the last two commands.

Ensure that the g++ version is 11.1.0:
```shell
g++ --version
```

NOTE: We can switch gcc/g++ versions using `sudo update-alternatives --config gcc`, if you need gcc-7.5.0 to build shenango in case you modified something.

## Building (our) shenango

Check gcc version is `7.5.0`.
```shell
gcc --version
```

Then, build shenango:
```shell
git clone -b feature/shenango-eventing https://github.com/saubhik/caladan.git && \
cd caladan && \
make submodules && \
make clean && make -j20
```

Now, you might need to switch to g++-11 to make bindings/cc.
Use the following:
```shell
sudo update-alternatives --config gcc
```

Now, continue with the building:
```shell
make -C bindings/cc && \
pushd ksched && \
make clean && make && \
popd && \
sudo ./scripts/setup_machine.sh
```

Then, start `iokerneld` in a separate process. I use `tmux`.
```shell
tmux new -s iokernel
sudo ./iokerneld
```
Then press Ctrl-B, D (after releasing Ctrl-B) to detach from tmux.
You can attach back using `tmux a`.

That's it for shenango.

## Building (our) folly

cd back to our NFS root.
```shell
cd /proj/quic-server-PG0/users/saubhik/tmp
```

First install the necessary system libraries.
```shell
sudo apt-get update
sudo apt-get install \
    g++ \
    cmake \
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
    libunwind-dev
```

Now we need to install few dependencies from source.

### Install googletest from source

Ensure we are in our NFS root.
```shell
cd /proj/quic-server-PG0/users/saubhik/tmp
```

```shell
wget https://github.com/google/googletest/archive/release-1.8.0.tar.gz && \
tar zxf release-1.8.0.tar.gz && \
rm -f release-1.8.0.tar.gz && \
cd googletest-release-1.8.0 && \
cmake . && \
make -j20 && \
sudo make install
```

### Install fmt from source

Ensure we are in our NFS root.
```shell
cd /proj/quic-server-PG0/users/saubhik/tmp
```

```shell
git clone https://github.com/fmtlib/fmt.git && \
cd fmt && \
mkdir _build && \
cd _build && \
cmake .. && \
make -j20 && \
sudo make install
```


### Install (our) folly

Ensure we are in our NFS root.
```shell
cd /proj/quic-server-PG0/users/saubhik/tmp
```

Clone folly:
```shell
git clone -b feature/finalize-network-io https://github.com/saubhik/folly.git
```

Change the lines 148-161 in `folly/CMake/folly-deps.cmake` to reflect the correct shenango path. Right now, it points to mine, as follows. Or you can keep using my shenango build if you're not modifying anything.
```shell
# add shenango headers
list(APPEND FOLLY_INCLUDE_DIRECTORIES "/proj/quic-server-PG0/users/saubhik/caladan/inc")
list(APPEND FOLLY_INCLUDE_DIRECTORIES "/proj/quic-server-PG0/users/saubhik/caladan/bindings/cc")

# link shenango libraries
list(APPEND FOLLY_LINK_LIBRARIES /proj/quic-server-PG0/users/saubhik/caladan/bindings/cc/librt++.a)
list(APPEND FOLLY_LINK_LIBRARIES /proj/quic-server-PG0/users/saubhik/caladan/libruntime.a)
list(APPEND FOLLY_LINK_LIBRARIES /proj/quic-server-PG0/users/saubhik/caladan/libnet.a)
list(APPEND FOLLY_LINK_LIBRARIES /proj/quic-server-PG0/users/saubhik/caladan/libbase.a)

list(APPEND FOLLY_LINK_LIBRARIES /usr/local/lib/libfmt.a)

# need to set the linker script
set(CMAKE_EXE_LINKER_FLAGS "-T /proj/quic-server-PG0/users/saubhik/caladan/base/base.ld")
```

Then, go ahead and install.
```shell
cd /proj/quic-server-PG0/users/saubhik/tmp && \
cd folly && \
mkdir _build && \
cd _build && \
cmake .. && \
make -j20 && \
sudo make install
```

That's it for folly!

## Building (our) mvfst

First, we need to install fizz.

### Install (our) fizz

Ensure we are in our NFS root.
```shell
cd /proj/quic-server-PG0/users/saubhik/tmp
```

Install the following dependencies.
```shell
sudo apt-get update && \
sudo apt-get install \
    g++ \
    cmake \
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

Clone fizz.
```shell
git clone -b feature/add-shenango-support https://github.com/saubhik/fizz
```

NOTE: You need to change lines 226-240 in fizz/fizz/CMakeLists.txt to reflect the correct shenango path. Right now it points to my folder, as follows.
```shell
SET(SHENANGO_LIBRARIES "")
SET(SHENANGO_INCLUDE_DIRECTORIES "")

# add shenango headers
list(APPEND SHENANGO_INCLUDE_DIRECTORIES "/proj/quic-server-PG0/users/saubhik/caladan/inc")
list(APPEND SHENANGO_INCLUDE_DIRECTORIES "/proj/quic-server-PG0/users/saubhik/caladan/bindings/cc")

# link shenango libraries
list(APPEND SHENANGO_LIBRARIES "/proj/quic-server-PG0/users/saubhik/caladan/bindings/cc/librt++.a")
list(APPEND SHENANGO_LIBRARIES "/proj/quic-server-PG0/users/saubhik/caladan/libruntime.a")
list(APPEND SHENANGO_LIBRARIES "/proj/quic-server-PG0/users/saubhik/caladan/libnet.a")
list(APPEND SHENANGO_LIBRARIES "/proj/quic-server-PG0/users/saubhik/caladan/libbase.a")

# need to set the linker script
set(CMAKE_EXE_LINKER_FLAGS "-T /proj/quic-server-PG0/users/saubhik/caladan/base/base.ld")
```

Then, go ahead and install.
```shell
cd /proj/quic-server-PG0/users/saubhik/tmp && \
cd fizz && \
mkdir _build && \
cd _build && \
cmake ../fizz && \
make -j20 && \
sudo make install
```

### Build (our) mvfst

Ensure we are in our NFS root.
```shell
cd /proj/quic-server-PG0/users/saubhik/tmp
```

Clone mvfst.
```shell
git clone -b feature/fix-compilation-issues https://github.com/saubhik/mvfst.git
```

You need to change line 15 of mvfst/CMakeLists.txt to reflect your shenango path.
Currently, it is set as follows. Change this, if you modified shenango.
```shell
# need to set the linker script
set(CMAKE_EXE_LINKER_FLAGS "-T /proj/quic-server-PG0/users/saubhik/caladan/base/base.ld")
```

The, go ahead and install.
```shell
cd /proj/quic-server-PG0/users/saubhik/tmp && \
cd mvfst && \
mkdir _build && \
cd _build && \
cmake -DBUILD_TESTS=ON .. && \
make -j20 && \
sudo make install
```

That's it, you have built mvfst!

### Running the mvfst Echo app

You can run the server as follows:
```shell
cd /proj/quic-server-PG0/users/saubhik/tmp/mvfst/_build && \
./quic/samples/echo -mode=server -host=10.10.1.1 -port=8000
```

You can run the client (on client node) as follows:
```shell
cd /proj/quic-server-PG0/users/saubhik/tmp/mvfst/_build && \
./quic/samples/echo -mode=client -host=10.10.1.1 -port=8000
```

When you modify anything in mvfst or folly, you can go to their `_build` directory and just do the following:
```shell
make -j20 && sudo make install
```
There is no need to do `cmake ..` or similar.

---
