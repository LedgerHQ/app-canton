from com.daml.ledger.api.v2.interactive import interactive_submission_common_data_pb2 as _interactive_submission_common_data_pb2
from com.daml.ledger.api.v2.interactive.transaction.v1 import interactive_submission_data_pb2 as _interactive_submission_data_pb2
from com.daml.ledger.api.v2.interactive.transaction.v1 import interactive_submission_data_cb_pb2 as _interactive_submission_data_cb_pb2
from com.daml.ledger.api.v2 import value_pb2 as _value_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Iterable as _Iterable, Mapping as _Mapping, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class DeviceDamlTransaction(_message.Message):
    __slots__ = ("version", "roots", "nodes_count", "node_seeds")
    class NodeSeed(_message.Message):
        __slots__ = ("node_id", "seed")
        NODE_ID_FIELD_NUMBER: _ClassVar[int]
        SEED_FIELD_NUMBER: _ClassVar[int]
        node_id: int
        seed: bytes
        def __init__(self, node_id: _Optional[int] = ..., seed: _Optional[bytes] = ...) -> None: ...
    class Node(_message.Message):
        __slots__ = ("node_id", "v1")
        NODE_ID_FIELD_NUMBER: _ClassVar[int]
        V1_FIELD_NUMBER: _ClassVar[int]
        node_id: str
        v1: _interactive_submission_data_pb2.Node
        def __init__(self, node_id: _Optional[str] = ..., v1: _Optional[_Union[_interactive_submission_data_pb2.Node, _Mapping]] = ...) -> None: ...
    VERSION_FIELD_NUMBER: _ClassVar[int]
    ROOTS_FIELD_NUMBER: _ClassVar[int]
    NODES_COUNT_FIELD_NUMBER: _ClassVar[int]
    NODE_SEEDS_FIELD_NUMBER: _ClassVar[int]
    version: str
    roots: _containers.RepeatedScalarFieldContainer[str]
    nodes_count: int
    node_seeds: _containers.RepeatedCompositeFieldContainer[DeviceDamlTransaction.NodeSeed]
    def __init__(self, version: _Optional[str] = ..., roots: _Optional[_Iterable[str]] = ..., nodes_count: _Optional[int] = ..., node_seeds: _Optional[_Iterable[_Union[DeviceDamlTransaction.NodeSeed, _Mapping]]] = ...) -> None: ...

class DeviceDamlTransactionDisplay(_message.Message):
    __slots__ = ("version", "roots", "nodes_count", "node_seeds")
    class NodeSeed(_message.Message):
        __slots__ = ("node_id", "seed")
        NODE_ID_FIELD_NUMBER: _ClassVar[int]
        SEED_FIELD_NUMBER: _ClassVar[int]
        node_id: int
        seed: bytes
        def __init__(self, node_id: _Optional[int] = ..., seed: _Optional[bytes] = ...) -> None: ...
    class Node(_message.Message):
        __slots__ = ("node_id", "v1")
        NODE_ID_FIELD_NUMBER: _ClassVar[int]
        V1_FIELD_NUMBER: _ClassVar[int]
        node_id: str
        v1: _interactive_submission_data_cb_pb2.NodeDisplay
        def __init__(self, node_id: _Optional[str] = ..., v1: _Optional[_Union[_interactive_submission_data_cb_pb2.NodeDisplay, _Mapping]] = ...) -> None: ...
    VERSION_FIELD_NUMBER: _ClassVar[int]
    ROOTS_FIELD_NUMBER: _ClassVar[int]
    NODES_COUNT_FIELD_NUMBER: _ClassVar[int]
    NODE_SEEDS_FIELD_NUMBER: _ClassVar[int]
    version: str
    roots: _containers.RepeatedScalarFieldContainer[str]
    nodes_count: int
    node_seeds: _containers.RepeatedCompositeFieldContainer[DeviceDamlTransactionDisplay.NodeSeed]
    def __init__(self, version: _Optional[str] = ..., roots: _Optional[_Iterable[str]] = ..., nodes_count: _Optional[int] = ..., node_seeds: _Optional[_Iterable[_Union[DeviceDamlTransactionDisplay.NodeSeed, _Mapping]]] = ...) -> None: ...

class DeviceMetadata(_message.Message):
    __slots__ = ("submitter_info", "synchronizer_id", "mediator_group", "transaction_uuid", "preparation_time", "input_contracts_count", "min_ledger_effective_time", "max_ledger_effective_time")
    class SubmitterInfo(_message.Message):
        __slots__ = ("act_as", "command_id")
        ACT_AS_FIELD_NUMBER: _ClassVar[int]
        COMMAND_ID_FIELD_NUMBER: _ClassVar[int]
        act_as: _containers.RepeatedScalarFieldContainer[str]
        command_id: str
        def __init__(self, act_as: _Optional[_Iterable[str]] = ..., command_id: _Optional[str] = ...) -> None: ...
    class GlobalKeyMappingEntry(_message.Message):
        __slots__ = ("key", "value")
        KEY_FIELD_NUMBER: _ClassVar[int]
        VALUE_FIELD_NUMBER: _ClassVar[int]
        key: _interactive_submission_common_data_pb2.GlobalKey
        value: _value_pb2.Value
        def __init__(self, key: _Optional[_Union[_interactive_submission_common_data_pb2.GlobalKey, _Mapping]] = ..., value: _Optional[_Union[_value_pb2.Value, _Mapping]] = ...) -> None: ...
    class InputContract(_message.Message):
        __slots__ = ("v1", "created_at", "driver_metadata")
        V1_FIELD_NUMBER: _ClassVar[int]
        CREATED_AT_FIELD_NUMBER: _ClassVar[int]
        DRIVER_METADATA_FIELD_NUMBER: _ClassVar[int]
        v1: _interactive_submission_data_pb2.Create
        created_at: int
        driver_metadata: bytes
        def __init__(self, v1: _Optional[_Union[_interactive_submission_data_pb2.Create, _Mapping]] = ..., created_at: _Optional[int] = ..., driver_metadata: _Optional[bytes] = ...) -> None: ...
    SUBMITTER_INFO_FIELD_NUMBER: _ClassVar[int]
    SYNCHRONIZER_ID_FIELD_NUMBER: _ClassVar[int]
    MEDIATOR_GROUP_FIELD_NUMBER: _ClassVar[int]
    TRANSACTION_UUID_FIELD_NUMBER: _ClassVar[int]
    PREPARATION_TIME_FIELD_NUMBER: _ClassVar[int]
    INPUT_CONTRACTS_COUNT_FIELD_NUMBER: _ClassVar[int]
    MIN_LEDGER_EFFECTIVE_TIME_FIELD_NUMBER: _ClassVar[int]
    MAX_LEDGER_EFFECTIVE_TIME_FIELD_NUMBER: _ClassVar[int]
    submitter_info: DeviceMetadata.SubmitterInfo
    synchronizer_id: str
    mediator_group: int
    transaction_uuid: str
    preparation_time: int
    input_contracts_count: int
    min_ledger_effective_time: int
    max_ledger_effective_time: int
    def __init__(self, submitter_info: _Optional[_Union[DeviceMetadata.SubmitterInfo, _Mapping]] = ..., synchronizer_id: _Optional[str] = ..., mediator_group: _Optional[int] = ..., transaction_uuid: _Optional[str] = ..., preparation_time: _Optional[int] = ..., input_contracts_count: _Optional[int] = ..., min_ledger_effective_time: _Optional[int] = ..., max_ledger_effective_time: _Optional[int] = ...) -> None: ...
