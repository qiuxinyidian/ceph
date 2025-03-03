===============================================
UADK Acceleration for Compression
===============================================

UADK is a framework that makes it possible for applications to access hardware
accelerators in a unified, secure, and efficient way. UADK is comprised of
UACCE, libwd, and many other algorithm libraries.

See `Compressor UADK Support`_.


UADK in the Software Stack
==========================

UADK is a general-purpose user space accelerator framework that uses Shared
Virtual Addressing (SVA) to provide a unified programming interface for
hardware acceleration of cryptographic and compression algorithms.

UADK includes Unified/User-space-access-intended Accelerator Framework (UACCE),
which enables hardware accelerators that support SVA to adapt to UADK.

Currently, HiSilicon Kunpeng hardware accelerators have been registered with
UACCE. Through the UADK framework, users can run cryptographic and compression
algorithms using hardware accelerators instead of CPUs, which frees up CPU
computing power and improves computing performance.

Users can access the hardware accelerators by performing user-mode operations
on the character devices, or the use of UADK can be achieved via frameworks
that have been enabled by others including UADK support (for example, OpenSSL*
libcrypto*, DPDK, and the Linux* Kernel Crypto Framework).

See `OpenSSL UADK Engine`_.

UADK Environment Setup
======================
UADK consists of UACCE, vendor drivers, and an algorithm layer. UADK requires
the hardware accelerator to support SVA, and the operating system to support
IOMMU and SVA. Hardware accelerators are registered as different character
devices with UACCE by kernel-mode drivers.

::

          +----------------------------------+
          |                apps              |
          +----+------------------------+----+
               |                        |
               |                        |
       +-------+--------+       +-------+-------+
       |   scheduler    |       | alg libraries |
       +-------+--------+       +-------+-------+
               |                         |
               |                         |
               |                         |
               |                +--------+------+
               |                | vendor drivers|
               |                +-+-------------+
               |                  |
               |                  |
            +--+------------------+--+
            |         libwd          |
    User    +----+-------------+-----+
    --------------------------------------------------
    Kernel    +--+-----+   +------+
              | uacce  |   | smmu |
              +---+----+   +------+
                  |
              +---+------------------+
              | vendor kernel driver |
              +----------------------+
    --------------------------------------------------
             +----------------------+
             |   HW Accelerators    |
             +----------------------+

Configuration
=============

#. Kernel Requirement

Users must ensure that UACCE is supported by the Linux kernel release in use,
which should be 5.9 or later with SVA (Shared Virtual Addressing) enabled.

UACCE may be built as a loadable module or built into the kernel. Here's an
example to build UACCE with hardware accelerators for the HiSilicon Kunpeng
platform.

    .. prompt:: bash $

       CONFIG_IOMMU_SVA_LIB=y
       CONFIG_ARM_SMMU=y
       CONFIG_ARM_SMMU_V3=y
       CONFIG_ARM_SMMU_V3_SVA=y
       CONFIG_PCI_PASID=y
       CONFIG_UACCE=y
       CONFIG_CRYPTO_DEV_HISI_QM=y
       CONFIG_CRYPTO_DEV_HISI_ZIP=y

Make sure all these above kernel configurations are selected.

#. UADK enablement
If the architecture is ``aarch64``, it will automatically download the UADK
source code to build the static library. When building on other CPU
architectures, the user may enable UADK by adding ``-DWITH_UADK=true`` to the
compilation command line options. Note that UADK may not be compatible with all
architectures.

#. Manually Building UADK
As implied in the above paragraph, if the architecture is ``aarch64``, the UADK
is enabled automatically and there is no need to build it manually. However,
below we provide the procedure for manually building UADK so that developers
can study how it is built. 

   .. prompt:: bash $ 

      git clone https://github.com/Linaro/uadk.git
      cd uadk
      mkdir build
      ./autogen.sh
      ./configure --prefix=$PWD/build
      make
      make install

   .. note:: Without ``--prefix``, UADK will be installed under
             ``/usr/local/lib`` by default. If you get the error: 
             ``cannot find -lnuma``, install the ``libnuma-dev`` package.

#. Configure

   Edit the Ceph configuration file (usually ``ceph.conf``) to enable UADK
   support for *zlib* compression::

         uadk_compressor_enabled=true

   The default value in `global.yaml.in` for `uadk_compressor_enabled` is
   ``false``.

.. _Compressor UADK Support: https://github.com/ceph/ceph/pull/58336
.. _OpenSSL UADK Engine: https://github.com/Linaro/uadk_engine
