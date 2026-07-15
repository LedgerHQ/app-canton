import json
import base64
import hashlib
from io import BytesIO
from typing import Optional, Union, List

from nacl.signing import SigningKey

# pylint: disable=no-name-in-module, import-error
from split_tx_util import (
    split_transaction,
)  # type: ignore

from com.digitalasset.canton.crypto.v30.crypto_pb2 import (
    CryptoKeyFormat,
    SigningKeyScheme,
    SigningKeySpec,
    SigningPublicKey,
    SigningKeyUsage,
)
from com.digitalasset.canton.protocol.v30.topology_pb2 import (
    TopologyMapping,
    NamespaceDelegation,
    PartyToKeyMapping,
    TopologyTransaction,
    Enums,
    PartyToParticipant,
)

# proto/com/digitalasset/canton/version/v1/untyped_versioned_message_pb2.pyi
from com.digitalasset.canton.version.v1.untyped_versioned_message_pb2 import (
    UntypedVersionedMessage,
)

from .canton_utils import read, read_uint, read_varint, write_varint, UINT64_MAX

PURPOSE_TOPOLOGY_TRANSACTION_SIGNATURE = 11
PURPOSE_PUBLIC_KEY_FINGERPRINT = 12
PURPOSE_MULTI_TOPOLOGY_TRANSACTION = 55

DEFAULT_PARTY_NAME = "ldg"


class TransactionError(Exception):
    pass


class Transaction:
    def __init__(self, nonce: int, to: Union[str, bytes], value: int, memo: str) -> None:
        self.nonce: int = nonce
        self.to: bytes = bytes.fromhex(to[2:]) if isinstance(to, str) else to
        self.value: int = value
        self.memo: bytes = memo.encode("ascii")

        if not 0 <= self.nonce <= UINT64_MAX:
            raise TransactionError(f"Bad nonce: '{self.nonce}'!")

        if not 0 <= self.value <= UINT64_MAX:
            raise TransactionError(f"Bad value: '{self.value}'!")

        if len(self.to) != 20:
            raise TransactionError(f"Bad address: '{self.to.hex()}'!")

    def serialize(self) -> bytes:
        return b"".join(
            [
                self.nonce.to_bytes(8, byteorder="big"),
                self.to,
                self.value.to_bytes(8, byteorder="big"),
                write_varint(len(self.memo)),
                self.memo,
            ]
        )

    @classmethod
    def from_bytes(cls, hexa: Union[bytes, BytesIO]):
        buf: BytesIO = BytesIO(hexa) if isinstance(hexa, bytes) else hexa

        nonce: int = read_uint(buf, 64, byteorder="big")
        to: bytes = read(buf, 20)
        value: int = read_uint(buf, 64, byteorder="big")
        memo_len: int = read_varint(buf)
        memo: str = read(buf, memo_len).decode("ascii")

        return cls(nonce=nonce, to=to, value=value, memo=memo)

    @classmethod
    def get_hash_from_json(cls, json_file: str) -> bytes:
        with open(json_file, "r", encoding="utf-8") as file:
            data = json.load(file)

        tx_hash = data.get("prepared_transaction_hash") or data.get("hash")
        # Detect if base64 or hex encoding
        if tx_hash:
            try:
                return bytes.fromhex(tx_hash)
            except ValueError:
                return base64.b64decode(tx_hash)
        raise TransactionError("No hash found in JSON file")

    @classmethod
    def serialize_from_json_into_tx_parts(cls, json_file: str) -> tuple[bytes, list[bytes], bytes, list[bytes]]:
        """Read JSON transaction file and serialize into parts."""
        return split_transaction(json_file)

    @classmethod
    def compute_sha256_canton_hash(cls, purpose: int, content: bytes):
        hash_purpose = purpose.to_bytes(4, byteorder="big")
        # Hashed content
        hashed_content = hashlib.sha256(hash_purpose + content).digest()

        # Multi-hash encoding
        # Canton uses an implementation of multihash (https://github.com/multiformats/multihash)
        # Since we use sha256 always here, we can just hardcode the prefixes
        # This may be improved and simplified in subsequent versions
        sha256_algorithm_prefix = bytes([0x12])
        sha256_length_prefix = bytes([0x20])

        print(f"\n %%%%% Canton Hash {(sha256_algorithm_prefix + sha256_length_prefix + hashed_content).hex()}")

        return sha256_algorithm_prefix + sha256_length_prefix + hashed_content

    @classmethod
    def compute_topology_transaction_hash(cls, serialized_versioned_transaction: bytes) -> bytes:
        """
        Computes the hash of a serialized topology transaction.

        Args:
            serialized_versioned_transaction (bytes): The serialized transaction data.

        Returns:
            bytes: The computed hash.
        """
        print(
            "\n>>>>Computing topology transaction hash for serialized transaction:",
            serialized_versioned_transaction.hex(),
        )
        return Transaction.compute_sha256_canton_hash(
            PURPOSE_TOPOLOGY_TRANSACTION_SIGNATURE, serialized_versioned_transaction
        )

    @classmethod
    def compute_multi_transaction_hash(cls, hashes: List[bytes]) -> bytes:
        """
        Computes a combined hash for multiple topology transactions.

        This function sorts the given hashes, concatenates them with length encoding,
        and computes a Canton-specific SHA-256 hash with a predefined purpose.

        Args:
            hashes (list[bytes]): A list of hashes representing individual topology transactions.

        Returns:
            bytes: The computed multi-transaction hash.
        """
        # Sort the hashes by their hex representation
        sorted_hashes = sorted(hashes, key=lambda h: h.hex())

        print("\nSorted hashes for multi-transaction hash computation:")
        for h in sorted_hashes:
            print(h.hex())

        # Start with the number of hashes encoded as a 4 bytes integer in big endian
        combined_hashes = len(sorted_hashes).to_bytes(4, byteorder="big")

        # Concatenate each hash, prefixing them with their size as a 4 bytes integer in big endian
        for h in sorted_hashes:
            combined_hashes += len(h).to_bytes(4, byteorder="big") + h

        print(f"\nConcatenated sorted hashes for multi-transaction hash computation: {combined_hashes.hex()}")

        return Transaction.compute_sha256_canton_hash(PURPOSE_MULTI_TOPOLOGY_TRANSACTION, combined_hashes)

    @classmethod
    def _build_topology_transaction(
        cls,
        mapping: TopologyMapping,
        serial: int,
    ) -> bytes:
        """
        Constructs a topology transaction with the given mapping and serial number.

        Args:
            mapping (TopologyMapping): The topology mapping to be included in the transaction.
            serial (int): The serial number for the transaction.

        Returns:
            bytes: Serialized topology transaction containing the provided mapping and operation.
        """
        topology_tx = TopologyTransaction(
            mapping=mapping,
            operation=Enums.TopologyChangeOp.TOPOLOGY_CHANGE_OP_ADD_REPLACE,
            serial=serial,
        )

        versioned_topology_tx = UntypedVersionedMessage(
            data=topology_tx.SerializeToString(),
            version=30,
        )

        return versioned_topology_tx.SerializeToString()

    @classmethod
    def _compute_party_fingerprint(cls, public_key: bytes) -> str:
        # Check if key is raw or DER format
        if len(public_key) == 32:
            raw_key = public_key
        elif len(public_key) == 44 and public_key.startswith(b"\x30\x2a\x30\x05\x06\x03\x2b\x65\x70\x03\x21\x00"):
            raw_key = public_key[12:]
        else:
            raise ValueError("Public key must be in raw (32 bytes) or DER (44 bytes) format")

        return cls.compute_sha256_canton_hash(PURPOSE_PUBLIC_KEY_FINGERPRINT, raw_key).hex()

    @classmethod
    def namespace_delegation(cls, public_key: bytes, der_format: bool) -> bytes:
        if der_format:
            key_format = CryptoKeyFormat.CRYPTO_KEY_FORMAT_DER_X509_SUBJECT_PUBLIC_KEY_INFO
        else:
            key_format = CryptoKeyFormat.CRYPTO_KEY_FORMAT_RAW
        key_scheme = SigningKeyScheme.SIGNING_KEY_SCHEME_ED25519
        key_spec = SigningKeySpec.SIGNING_KEY_SPEC_EC_CURVE25519

        print(f"\nPublic key for namespace delegation: {public_key.hex()}\n")

        signing_public_key = SigningPublicKey(
            format=key_format,
            public_key=public_key,
            scheme=key_scheme,
            key_spec=key_spec,
            usage=[
                SigningKeyUsage.SIGNING_KEY_USAGE_NAMESPACE,
                SigningKeyUsage.SIGNING_KEY_USAGE_PROTOCOL,
            ],
        )

        # Generate random namespace private ED25519 key for the party
        private_key = SigningKey.generate()
        public_key_bytes = private_key.verify_key.encode()

        namespace = cls._compute_party_fingerprint(public_key_bytes)

        namespace_delegation_mapping = TopologyMapping(
            namespace_delegation=NamespaceDelegation(
                namespace=namespace,
                target_key=signing_public_key,
                is_root_delegation=True,
            )
        )

        return cls._build_topology_transaction(
            mapping=namespace_delegation_mapping,
            serial=1,
        )

    @classmethod
    def party_to_key(cls, public_key: bytes, der_format: bool, party_id: Optional[str] = None) -> bytes:
        if der_format:
            key_format = CryptoKeyFormat.CRYPTO_KEY_FORMAT_DER_X509_SUBJECT_PUBLIC_KEY_INFO
        else:
            key_format = CryptoKeyFormat.CRYPTO_KEY_FORMAT_RAW
        key_scheme = SigningKeyScheme.SIGNING_KEY_SCHEME_ED25519
        key_spec = SigningKeySpec.SIGNING_KEY_SPEC_EC_CURVE25519

        signing_public_key = SigningPublicKey(
            format=key_format,
            public_key=public_key,
            scheme=key_scheme,
            key_spec=key_spec,
            usage=[
                SigningKeyUsage.SIGNING_KEY_USAGE_NAMESPACE,
                SigningKeyUsage.SIGNING_KEY_USAGE_PROTOCOL,
            ],
        )

        if party_id is None:
            party_fingerprint = cls._compute_party_fingerprint(public_key)
            party_id = DEFAULT_PARTY_NAME + "::" + party_fingerprint

        party_to_key_mapping = TopologyMapping(
            party_to_key_mapping=PartyToKeyMapping(
                party=party_id,
                threshold=1,
                signing_keys=[signing_public_key],
            )
        )

        return cls._build_topology_transaction(
            mapping=party_to_key_mapping,
            serial=2,
        )

    @classmethod
    def party_to_participant(cls, public_key: bytes, validators_seeds: list[bytes]) -> bytes:
        party_fingerprint = cls._compute_party_fingerprint(public_key)
        party_id = DEFAULT_PARTY_NAME + "::" + party_fingerprint

        validators: List[PartyToParticipant.HostingParticipant] = []
        validators_count = len(validators_seeds)
        # Generate validators
        for i in range(validators_count):
            # Assert seed is 32 bytes
            assert len(validators_seeds[i]) == 32, "Validator seed must be 32 bytes"
            # Generate random participant private ED25519 key for the validator
            private_key = SigningKey(validators_seeds[i])
            public_key_bytes = private_key.verify_key.encode()
            participant_fingerprint = cls._compute_party_fingerprint(public_key_bytes)
            participant_id = "participant" + str(i + 1) + "::" + participant_fingerprint
            validators.append(
                PartyToParticipant.HostingParticipant(
                    participant_uid=participant_id,
                    permission=Enums.ParticipantPermission.PARTICIPANT_PERMISSION_CONFIRMATION,
                )
            )

        threshold = validators_count - 1 if validators_count > 1 else 1

        party_to_participant_mapping = TopologyMapping(
            party_to_participant=PartyToParticipant(
                party=party_id,
                threshold=threshold,
                participants=validators,
            )
        )

        return cls._build_topology_transaction(
            mapping=party_to_participant_mapping,
            serial=1,
        )

    @classmethod
    def party_to_participant_from_uid(
        cls,
        public_key: bytes,
        participant_uid: list[str],
        threshold: Optional[int] = None,
        party_id: Optional[str] = None,
    ) -> bytes:
        if party_id is None:
            party_fingerprint = cls._compute_party_fingerprint(public_key)
            party_id = DEFAULT_PARTY_NAME + "::" + party_fingerprint

        validators: List[PartyToParticipant.HostingParticipant] = []
        validators_count = len(participant_uid)

        for uid in participant_uid:
            participant = PartyToParticipant.HostingParticipant(
                participant_uid=uid,
                permission=Enums.ParticipantPermission.PARTICIPANT_PERMISSION_CONFIRMATION,
            )
            validators.append(participant)

        threshold = threshold if threshold is not None else validators_count

        party_to_participant_mapping = TopologyMapping(
            party_to_participant=PartyToParticipant(
                party=party_id,
                threshold=threshold,
                participants=validators,
            )
        )

        return cls._build_topology_transaction(
            mapping=party_to_participant_mapping,
            serial=1,
        )
