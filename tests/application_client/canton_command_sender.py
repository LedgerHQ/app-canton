from enum import IntEnum
from typing import Generator, List, Optional
from contextlib import contextmanager

from ragger.backend.interface import BackendInterface, RAPDU
from ragger.bip import pack_derivation_path


MAX_APDU_LEN: int = 255

CLA: int = 0xE0


class P1(IntEnum):
    P1_NONE = 0x00
    P1_CONFIRM = 0x01


class P1SignType(IntEnum):
    P1_SIGN_HASH = 0x00
    P1_SIGN_UNTYPED_VERSIONED_MESSAGE = 0x01
    P1_SIGN_PREPARED_TRANSACTION = 0x02


class P2(IntEnum):
    P2_NONE = 0x00
    P2_FIRST = 0x01
    P2_MORE = 0x02
    P2_MSG_END = 0x04


class InsType(IntEnum):
    GET_VERSION = 0x03
    GET_APP_NAME = 0x04
    GET_PUBLIC_KEY = 0x05
    SIGN_TX = 0x06


class Errors(IntEnum):
    SW_DENY = 0x6985
    SW_INCORRECT_DATA = 0x6A80
    SW_WRONG_P1P2 = 0x6A86
    SW_WRONG_DATA_LENGTH = 0x6A87
    SW_INS_NOT_SUPPORTED = 0x6D00
    SW_CLA_NOT_SUPPORTED = 0x6E00
    SW_WRONG_RESPONSE_LENGTH = 0xB000
    SW_DISPLAY_BIP32_PATH_FAIL = 0xB001
    SW_DISPLAY_ADDRESS_FAIL = 0xB002
    SW_DISPLAY_AMOUNT_FAIL = 0xB003
    SW_WRONG_TX_LENGTH = 0xB004
    SW_TX_PARSING_FAIL = 0xB005
    SW_TX_HASH_FAIL = 0xB006
    SW_BAD_STATE = 0xB007
    SW_SIGNATURE_FAIL = 0xB008
    SW_CHALLENGE_SIGNATURE_FAIL = 0xB009
    SW_TOPOLOGY_MULTIPLE_NAMESPACE_DELEGATIONS = 0xC101
    SW_TOPOLOGY_NULL_NAMESPACE_DELEGATION = 0xC102
    SW_TOPOLOGY_MISSING_TARGET_KEY = 0xC103
    SW_TOPOLOGY_PARTY_KEY_MISMATCH = 0xC104
    SW_TOPOLOGY_PARTY_KEY_WRONG_FORMAT = 0xC105
    SW_TOPOLOGY_MISSING_PARTY_KEY = 0xC106
    SW_TOPOLOGY_PARTY_ID_MISMATCH = 0xC201
    SW_TOPOLOGY_NO_SIGNING_KEYS = 0xC202
    SW_TOPOLOGY_MISSING_PARTY = 0xC301
    SW_TOPOLOGY_UNEXPECTED_NUMBER_OF_PARTICIPANTS = 0xC302
    SW_TOPOLOGY_UNEXPECTED_DUPLICATE_PARTICIPANT = 0xC303
    SW_TOPOLOGY_MISSING_PARTICIPANT_DATA = 0xC304
    SW_TOPOLOGY_UNEXPECTED_PARTICIPANT_ID = 0xC305
    SW_TOPOLOGY_UNEXPECTED_THRESHOLD_VALUE = 0xC306
    SW_TOPOLOGY_UNKNOWN_MAPPING_TYPE = 0xC401
    SW_TOPOLOGY_UNSUPPORTED_OPERATION = 0xC402
    SW_TOPOLOGY_MANDATORY_FIELD_MISSING = 0xC403


def split_message(message: bytes, max_size: int) -> List[bytes]:
    return [message[x : x + max_size] for x in range(0, len(message), max_size)]


class CantonCommandSender:
    def __init__(self, backend: BackendInterface) -> None:
        self.backend = backend

    def get_app_and_version(self) -> RAPDU:
        return self.backend.exchange(
            cla=0xB0,  # specific CLA for BOLOS
            ins=0x01,  # specific INS for get_app_and_version
            p1=P1.P1_NONE,
            p2=P2.P2_NONE,
            data=b"",
        )

    def get_version(self) -> RAPDU:
        return self.backend.exchange(cla=CLA, ins=InsType.GET_VERSION, p1=P1.P1_NONE, p2=P2.P2_NONE, data=b"")

    def get_app_name(self) -> RAPDU:
        return self.backend.exchange(cla=CLA, ins=InsType.GET_APP_NAME, p1=P1.P1_NONE, p2=P2.P2_NONE, data=b"")

    def get_public_key(self, path: str) -> RAPDU:
        return self.backend.exchange(
            cla=CLA,
            ins=InsType.GET_PUBLIC_KEY,
            p1=P1.P1_NONE,
            p2=P2.P2_NONE,
            data=pack_derivation_path(path),
        )

    @contextmanager
    def get_public_key_with_confirmation(self, path: str) -> Generator[None, None, None]:
        with self.backend.exchange_async(
            cla=CLA,
            ins=InsType.GET_PUBLIC_KEY,
            p1=P1.P1_CONFIRM,
            p2=P2.P2_NONE,
            data=pack_derivation_path(path),
        ) as response:
            yield response

    @contextmanager
    def sign_tx(self, path: str, transaction: bytes, p1: P1SignType) -> Generator[None, None, None]:
        print(f"Signing transaction with path: {path} and transaction length: {len(transaction)} bytes")
        self.backend.exchange(
            cla=CLA,
            ins=InsType.SIGN_TX,
            p1=p1,
            p2=P2.P2_FIRST | P2.P2_MORE,
            data=pack_derivation_path(path),
        )
        messages = split_message(transaction, MAX_APDU_LEN)

        for msg in messages[:-1]:
            self.backend.exchange(cla=CLA, ins=InsType.SIGN_TX, p1=p1, p2=P2.P2_MORE, data=msg)
        with self.backend.exchange_async(
            cla=CLA, ins=InsType.SIGN_TX, p1=p1, p2=P2.P2_MSG_END, data=messages[-1]
        ) as response:
            yield response

    @contextmanager
    def sign_topology_tx(
        self, path: str, transactions: List[bytes], challenge: Optional[bytes] = None
    ) -> Generator[None, None, None]:
        print(f"Signing topology transaction with path: {path} and {len(transactions)} transactions")
        p1 = P1SignType.P1_SIGN_UNTYPED_VERSIONED_MESSAGE

        challenge_data: bytes = b""
        if challenge:
            assert len(challenge) == 24, "Challenge must be 24 bytes long (16 bytes random + 8 bytes timestamp)"
            challenge_data = (len(challenge)).to_bytes(1, byteorder="big")
            challenge_data += challenge
        data = pack_derivation_path(path) + challenge_data

        self.backend.exchange(cla=CLA, ins=InsType.SIGN_TX, p1=p1, p2=P2.P2_FIRST | P2.P2_MORE, data=data)

        for tx in transactions[:-1]:
            print(f"Sending topology transaction chunk of length: {len(tx)} bytes")
            self._send_data_chunks(tx, p1, "UntypedVersionedMessage")

        last_tx = split_message(transactions[-1], MAX_APDU_LEN)

        self._send_message_chunks(last_tx[:-1], p1, "UntypedVersionedMessage")

        with self.backend.exchange_async(
            cla=CLA, ins=InsType.SIGN_TX, p1=p1, p2=P2.P2_MSG_END, data=last_tx[-1]
        ) as response:
            yield response

    @contextmanager
    def sign_tx_in_parts(
        self,
        path: str,
        daml_transaction: bytes,
        nodes: List[bytes],
        metadata: bytes,
        input_contracts: List[bytes],
    ) -> Generator[None, None, None]:
        print(f"Signing transaction (in parts) with path: {path}")
        p1 = P1SignType.P1_SIGN_PREPARED_TRANSACTION

        # Send derivation path
        self.backend.exchange(
            cla=CLA,
            ins=InsType.SIGN_TX,
            p1=p1,
            p2=P2.P2_FIRST | P2.P2_MORE,
            data=pack_derivation_path(path),
        )

        # Send DAML transaction
        self._send_data_chunks(daml_transaction, p1, "DamlTransaction")

        # Send nodes
        print(f"Sending {len(nodes)} Nodes")
        for node_id, node_data in enumerate(nodes):
            print(f"Sending node {node_id} of {len(nodes)}")
            self._send_data_chunks(node_data, p1, f"Node {node_id}")

        # Send metadata
        final_chunk = None
        # Skip sending last chunk if Metadata is last message
        more_contracts = len(input_contracts) != 0
        final_chunk = self._send_data_chunks(metadata, p1, "Metadata", more_contracts)

        # Send input contracts
        print(f"Sending {len(input_contracts)} InputContracts")
        for i, contract_data in enumerate(input_contracts):
            last_contract = i == len(input_contracts) - 1
            final_chunk = self._send_data_chunks(contract_data, p1, "InputContract", not last_contract)

        if final_chunk is None:
            final_chunk = b""

        with self.backend.exchange_async(
            cla=CLA, ins=InsType.SIGN_TX, p1=p1, p2=P2.P2_MSG_END, data=final_chunk
        ) as response:
            yield response

    def _send_data_chunks(self, data: bytes, p1: int, data_type: str, send_last_chunk=True) -> Optional[bytes]:
        """Send data in chunks with appropriate messaging."""
        messages = split_message(data, MAX_APDU_LEN)
        print(f"Sending {len(messages)} chunks of {data_type} data")

        self._send_message_chunks(messages[:-1], p1, data_type)

        if send_last_chunk:
            print(f"Sending last {data_type} chunk {len(messages)} of {len(messages)}")
            # Send final chunk with MSG_END flag
            self.backend.exchange(
                cla=CLA,
                ins=InsType.SIGN_TX,
                p1=p1,
                p2=P2.P2_MORE | P2.P2_MSG_END,
                data=messages[-1],
            )
            return None

        return messages[-1]

    def _send_message_chunks(self, messages: List[bytes], p1: int, data_type: str) -> None:
        """Send intermediate message chunks (all but the last one)."""
        for chunk_id, msg in enumerate(messages, start=1):
            print(f"Sending {data_type} chunk {chunk_id} of {len(messages) + 1}")
            self.backend.exchange(cla=CLA, ins=InsType.SIGN_TX, p1=p1, p2=P2.P2_MORE, data=msg)

    def get_async_response(self) -> Optional[RAPDU]:
        return self.backend.last_async_response
