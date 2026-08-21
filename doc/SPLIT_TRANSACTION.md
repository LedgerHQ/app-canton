# How to split protobuf transaction into separate components

## Description

Due to the limited RAM available on the devices, it is not feasible to parse and compute the hash of the entire  protobuf message at once.
Therefore, the message is divided into smaller protobuf components to enable efficient processing.

The protobuf message `PreparedTransaction` is divided into the following components ([see original schema here](https://github.com/digital-asset/canton/blob/main/community/ledger-api/src/main/protobuf/com/daml/ledger/api/v2/interactive/interactive_submission_service.proto)):

1. DAML transaction data, excluding the node list (`DamlTransaction` in the schema)
2. One or more transaction nodes (`DamlTransaction.Node` in the schema)
3. Metadata, excluding the input contract list (`Metadata` in the schema)
4. Zero or more input contracts (`Metadata.InputContract` in the schema)


## Splitting into components

### Preparation: Removing Unused Blob Fields from `InputContract`

To reduce the size of the resulting components, blob fields that are not used for hash computation and clear sign must be removed.
Remove the `eventBlob` field from `InputContract`.

### Split and Update `DamlTransaction`

Next, the `DamlTransaction` message contained within the `PreparedTransaction` message is divided.
Since the `nodes` field occupies the most space, it is reasonable to separate each node into an individual `DamlTransaction.Nodes` message.

To achieve this, the `nodes` field is replaced with a new field `nodes_count` of type `int32`.
The `nodes_count` field stores the number of nodes that were previously contained in the removed `nodes` field.

As a result, after this modification, the updated version of `DamlTransaction` is defined as follows:

```proto
    message DeviceDamlTransaction {
        string version = 1;
        repeated string roots = 2;
        int32 nodes_count = 3; // <<=== REPLACED HERE
        repeated NodeSeed node_seeds = 4;
    }
```

The `DamlTransaction.Node`s that were previously part of the removed `nodes` field are now represented as separate components (messages) and are transmitted independently.


### Split and Update `Metadata`

The similar approach is applied to the `Metadata` message.
The `input_contracts` field is replaced with a new field `input_contracts_count` which stores number of elements that were previously contained in the `input_contracts` field.

The resulting `DeviceMetadata` message proto:

```proto
    message DeviceMetadata {
        reserved 1;

        SubmitterInfo submitter_info = 2;
        string synchronizer_id = 3;
        uint32 mediator_group = 4;
        string transaction_uuid = 5;
        uint64 preparation_time = 6;
        int32 input_contracts_count = 7; // <<=== REPLACED HERE
        optional uint64 min_ledger_effective_time = 9;
        optional uint64 max_ledger_effective_time = 10;
        repeated GlobalKeyMappingEntry global_key_mapping = 8;
    }
```

### Device proto file

As a more formal description of these modifications, a device proto file is also provided and can be found [HERE](../proto/device.proto).


## Sending Order

The resulting components (protobuf messages) are framed and transmitted to the device as described in [APDU.md](./APDU.md#sign_prepared_transaction-p1--0x02-example).

The transmission order is as follows:

1. `DeviceDamlTransaction`
2. One or more `DeviceDamlTransaction.Node` (see below; these must be ordered in a specific way)
3. `DeviceMetadata`
4. Zero or more `DeviceMetadata.InputContract` (in the same order as in the original list)

### Special Case: Ordering of `DeviceDamlTransaction.Node` Messages

Since the nodes in Canton are organized in a tree structure, the hash of a node cannot be computed until the hashes of all its children have been computed recursively.
Therefore, for each root node (roots are specified in the `roots` field of `DeviceDamlTransaction`), which represents a tree, the leaf nodes of that tree **shall be transmitted first**, followed by their parents, and so on, up to the root node.

After all nodes of the current root node’s tree have been transmitted, the transmission shall continue with the next root node’s tree as specified in the `roots` field of `DeviceDamlTransaction`.

For example, consider 2 root nodes in `DeviceDamlTransaction.roots` field `["1", "6"]`:

```ascii

Tree with root 0:   (0)       Tree with root 5:   (5)
                   /   \                         /   \
                 (1)   (2)                     (6)   (7)
                /   \                               /   \
              (3)   (4)                           (8)   (9)

```

The sending order of nodes could be: 3, 4, 1, 2, 0, 8, 9, 7, 6, 5.

An alternative valid order is:       4, 3, 1, 2, 0, 9, 8, 7, 6, 5.
