# Open CGNat

There is an open implementation of Carrier-Grade NAT. Written using the Intel DPDK Library. Opencgat act as a network bridge to achieve best PPS/BPS performance.

However, it is a WIP (Work In Progress), so do NOT use this project in production at the moment. I am open to proposals, issues, feature requests, etc.

### Features:
 * Port-Block Allocations (PBA) with logging
 * Endpoint-Independent Filtering
 * Endpoint-Independent Mapping
 * Performance depends on counts of threads on a machine

### TODO:
 * Support IPFIX/NetFlow export for cgnat events
 * Support for multiple cgnat pools and classifiers