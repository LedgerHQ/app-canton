#!/bin/bash

# Simple nanopb generation script that avoids generator modification
set -e -o pipefail

# Define color functions
red() { echo -e "\e[31m$*\e[0m"; }
green() { echo -e "\e[32m$*\e[0m"; }
yellow() { echo -e "\e[33m$*\e[0m"; }

ROOT_PATH=$(git rev-parse --show-toplevel)/proto
LEDGER_API_PROTO_PATH=$ROOT_PATH/canton-protos-scala/src/main/protobuf
COMMUNITY_PROTO_PATH=$ROOT_PATH/canton-protos-scala/src/main/protobuf
LAPI_VALUE_PROTO_PATH=$ROOT_PATH/canton-protos-scala/src/main/protobuf/com/daml/ledger/api/v2/value.proto
COMMUNITY_CANTON_PROTO_PATH=$COMMUNITY_PROTO_PATH/com/digitalasset/canton
PROTOCOL_PROTO_PATH=$COMMUNITY_CANTON_PROTO_PATH/protocol/v30
CRYPTO_PROTO_PATH=$COMMUNITY_CANTON_PROTO_PATH/crypto/v30
LEDGER_API_V2_PATH=$LEDGER_API_PROTO_PATH/com/daml/ledger/api/v2
OUTPUT_DIR="./"
NANOPB_GENERATOR="../vendor/nanopb/generator/protoc-gen-nanopb"
PROTOC="../vendor/nanopb/generator/protoc"
PROTO_SOURCE_REPO_URL="git@github.com:LedgerHQ/canton-protos-scala.git"
PROTO_SOURCE_REPO_REF="v1.4.0"

# Download utility
download_if_not_exists() {
  local url=$1
  local file_path=$2
  if [ ! -f "$file_path" ]; then
    echo "Downloading $file_path"
    mkdir -p "$(dirname "$file_path")"
    curl -s "$url" -o "$file_path"
  fi
}

# Define function that does the previous if / else logic
clone_if_not_exists() {
  local repo_url=$1
  local repo_ref=$2
  local sparse_path=$3
  local repo_name=$(basename "$repo_url" .git)

  if [ ! -d "$repo_name" ]; then
    yellow "Cloning $repo_name repository..."
    git clone --filter=blob:none --sparse --branch "$repo_ref" "$repo_url" && \
    cd "$repo_name" && \
    git sparse-checkout set "$sparse_path"
    cd ..
  else
    green "$repo_name repository already cloned."
    cd "$repo_name" && \
    git sparse-checkout add "$sparse_path"
    cd ..
  fi
}

# Setup dependencies
echo "Setting up dependencies..."
download_if_not_exists "https://raw.githubusercontent.com/protocolbuffers/protobuf/407aa2d9319f5db12964540810b446fecc22d419/src/google/protobuf/empty.proto" "google/protobuf/empty.proto"
download_if_not_exists "https://raw.githubusercontent.com/googleapis/googleapis/3597f7db2191c00b100400991ef96e52d62f5841/google/rpc/status.proto" "google/rpc/status.proto"
download_if_not_exists "https://raw.githubusercontent.com/googleapis/googleapis/9415ba048aa587b1b2df2b96fc00aa009c831597/google/rpc/error_details.proto" "google/rpc/error_details.proto"
download_if_not_exists "https://raw.githubusercontent.com/protocolbuffers/protobuf/refs/heads/main/src/google/protobuf/any.proto" "google/protobuf/any.proto"
download_if_not_exists "https://raw.githubusercontent.com/protocolbuffers/protobuf/refs/heads/main/src/google/protobuf/duration.proto" "google/protobuf/duration.proto"
download_if_not_exists "https://raw.githubusercontent.com/protocolbuffers/protobuf/refs/heads/main/src/google/protobuf/timestamp.proto" "google/protobuf/timestamp.proto"

clone_if_not_exists "$PROTO_SOURCE_REPO_URL" "$PROTO_SOURCE_REPO_REF" "src/main/protobuf/com/daml/ledger/api/v2"
clone_if_not_exists "$PROTO_SOURCE_REPO_URL" "$PROTO_SOURCE_REPO_REF" "src/main/protobuf/com/daml/ledger/api/v2/interactive"
clone_if_not_exists "$PROTO_SOURCE_REPO_URL" "$PROTO_SOURCE_REPO_REF" "src/main/protobuf/com/digitalasset/canton/version/v1"
clone_if_not_exists "$PROTO_SOURCE_REPO_URL" "$PROTO_SOURCE_REPO_REF" "src/main/protobuf/com/digitalasset/canton/protocol/v30"
clone_if_not_exists "$PROTO_SOURCE_REPO_URL" "$PROTO_SOURCE_REPO_REF" "src/main/protobuf/com/digitalasset/canton/crypto/v30"

mkdir -p "com/daml/ledger/api/v2" && cp "$LAPI_VALUE_PROTO_PATH" "com/daml/ledger/api/v2/value.proto"

# Copy device proto file
cp -v $ROOT_PATH/device.proto $LEDGER_API_V2_PATH/interactive/
cp -v $ROOT_PATH/interactive_submission_data_cb.proto $LEDGER_API_V2_PATH/interactive/transaction/v1/
cp -v $ROOT_PATH/value_cb.proto $ROOT_PATH/com/daml/ledger/api/v2/

# Create the options file for value.proto
echo "Creating value.options file..."
cat > value.options << 'EOF'
* anonymous_oneof:true
# Handle recursive Value fields with pointers to break cycles
com.daml.ledger.api.v2.RecordField.value type:FT_POINTER
com.daml.ledger.api.v2.List.elements type:FT_POINTER
com.daml.ledger.api.v2.Optional.value type:FT_POINTER
com.daml.ledger.api.v2.Variant.value type:FT_POINTER
com.daml.ledger.api.v2.TextMap.Entry.value type:FT_POINTER
com.daml.ledger.api.v2.TextMap.entries type:FT_POINTER
com.daml.ledger.api.v2.GenMap.Entry.value type:FT_POINTER
com.daml.ledger.api.v2.GenMap.Entry.key type:FT_POINTER
com.daml.ledger.api.v2.GenMap.entries type:FT_POINTER
com.daml.ledger.api.v2.Value.numeric type:FT_POINTER
com.daml.ledger.api.v2.Value.party type:FT_POINTER
com.daml.ledger.api.v2.Value.text type:FT_POINTER
com.daml.ledger.api.v2.Value.contract_id type:FT_POINTER
com.daml.ledger.api.v2.Identifier.package_id type:FT_POINTER
com.daml.ledger.api.v2.Identifier.module_name type:FT_POINTER
com.daml.ledger.api.v2.Identifier.entity_name type:FT_POINTER
com.daml.ledger.api.v2.Variant.constructor type:FT_POINTER
com.daml.ledger.api.v2.Enum.constructor type:FT_POINTER
com.daml.ledger.api.v2.RecordField.label type:FT_POINTER
com.daml.ledger.api.v2.TextMap.Entry.key type:FT_POINTER
com.daml.ledger.api.v2.Record.fields type:FT_POINTER
EOF

echo "Creating interactive_submission_data.options file..."
cat > interactive_submission_data.options << 'EOF'
* anonymous_oneof:true
# Handle recursive fields in interactive submission data
com.daml.ledger.api.v2.interactive.transaction.v1.Create.lf_version type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Create.contract_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Create.package_name type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Create.signatories type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Create.stakeholders type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Exercise.lf_version type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Exercise.contract_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Exercise.package_name type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Exercise.choice_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Exercise.signatories type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Exercise.stakeholders type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Exercise.acting_parties type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Exercise.children type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Exercise.choice_observers type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Fetch.lf_version type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Fetch.contract_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Fetch.package_name type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Fetch.signatories type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Fetch.stakeholders type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Fetch.acting_parties type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Fetch.interface_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Exercise.interface_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Rollback.children type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.Node submsg_callback:true
EOF

echo "Creating interactive_submission_service.options file..."
cat > interactive_submission_service.options << 'EOF'
* anonymous_oneof:true
com.daml.ledger.api.v2.interactive.PrepareSubmissionResponse.prepared_transaction_hash max_size: 32
com.daml.ledger.api.v2.interactive.PrepareSubmissionResponse.hashing_details type:FT_POINTER
com.daml.ledger.api.v2.interactive.ExecuteSubmissionRequest.submission_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.ExecuteSubmissionRequest.user_id type:FT_POINTER
EOF

echo "device.options file..."
cat > device.options << 'EOF'
* anonymous_oneof:true
com.daml.ledger.api.v2.interactive.DeviceDamlTransaction.NodeSeed.node_id type:FT_STATIC
com.daml.ledger.api.v2.interactive.DeviceDamlTransaction.version type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceDamlTransaction.roots type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceDamlTransaction.roots type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceDamlTransaction.node_seeds type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceDamlTransaction.Node.node_id type:FT_CALLBACK
com.daml.ledger.api.v2.interactive.DeviceDamlTransaction.Node submsg_callback:true
com.daml.ledger.api.v2.interactive.DeviceDamlTransaction.NodeSeed type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceMetadata.synchronizer_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceMetadata.transaction_uuid type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceMetadata.SubmitterInfo.act_as type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceMetadata.SubmitterInfo.command_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceMetadata.InputContract submsg_callback:true
com.daml.ledger.api.v2.interactive.DeviceMetadata.InputContract.driver_metadata type:FT_IGNORE

com.daml.ledger.api.v2.interactive.DeviceDamlTransactionDisplay.NodeSeed.node_id type:FT_STATIC
com.daml.ledger.api.v2.interactive.DeviceDamlTransactionDisplay.version type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceDamlTransactionDisplay.roots type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceDamlTransactionDisplay.roots type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceDamlTransactionDisplay.node_seeds type:FT_POINTER
com.daml.ledger.api.v2.interactive.DeviceDamlTransactionDisplay.Node.node_id type:FT_CALLBACK
com.daml.ledger.api.v2.interactive.DeviceDamlTransactionDisplay.Node submsg_callback:true
com.daml.ledger.api.v2.interactive.DeviceDamlTransactionDisplay.NodeSeed type:FT_POINTER
EOF

# CALLBACK versions of the options files
echo "Creating value_cb.options file..."
cat > value_cb.options << 'EOF'
* anonymous_oneof:true
# Handle recursive Value fields with pointers to break cycles
com.daml.ledger.api.v2.cb.Value submsg_callback:true
com.daml.ledger.api.v2.cb.RecordField.value type:FT_STATIC
com.daml.ledger.api.v2.cb.List.elements type:FT_CALLBACK
com.daml.ledger.api.v2.cb.Optional.value type:FT_CALLBACK
com.daml.ledger.api.v2.cb.TextMap.entries type:FT_CALLBACK
com.daml.ledger.api.v2.cb.TextMap.Entry.key type:FT_CALLBACK
com.daml.ledger.api.v2.cb.TextMap.Entry.value type:FT_STATIC
com.daml.ledger.api.v2.cb.GenMap.Entry.value type:FT_STATIC
com.daml.ledger.api.v2.cb.GenMap.Entry.key type:FT_STATIC
com.daml.ledger.api.v2.cb.GenMap.entries type:FT_CALLBACK
com.daml.ledger.api.v2.cb.Value.numeric type:FT_POINTER
com.daml.ledger.api.v2.cb.Value.party type:FT_POINTER
com.daml.ledger.api.v2.cb.Value.text type:FT_POINTER
com.daml.ledger.api.v2.cb.Value.contract_id type:FT_POINTER
com.daml.ledger.api.v2.cb.Identifier.package_id type:FT_POINTER
com.daml.ledger.api.v2.cb.Identifier.module_name type:FT_POINTER
com.daml.ledger.api.v2.cb.Identifier.entity_name type:FT_POINTER
com.daml.ledger.api.v2.cb.Variant.variant_id type:FT_CALLBACK
com.daml.ledger.api.v2.cb.Variant.constructor type:FT_CALLBACK
com.daml.ledger.api.v2.cb.Variant.value type:FT_CALLBACK
com.daml.ledger.api.v2.cb.Enum.constructor type:FT_POINTER
com.daml.ledger.api.v2.cb.RecordField.label type:FT_CALLBACK
com.daml.ledger.api.v2.cb.Record.fields type:FT_CALLBACK
com.daml.ledger.api.v2.cb.Record.record_id type:FT_CALLBACK
EOF

echo "Creating interactive_submission_data_cb.options file..."
cat > interactive_submission_data_cb.options << 'EOF'
* anonymous_oneof:true
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Create.lf_version type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Create.contract_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Create.package_name type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Create.signatories type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Create.stakeholders type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Create.argument type:FT_CALLBACK
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Exercise.lf_version type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Exercise.contract_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Exercise.package_name type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Exercise.choice_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Exercise.chosen_value type:FT_CALLBACK
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Exercise.signatories type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Exercise.stakeholders type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Exercise.acting_parties type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Exercise.children type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Exercise.exercise_result type:FT_CALLBACK
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Exercise.choice_observers type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Fetch.lf_version type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Fetch.contract_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Fetch.package_name type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Fetch.signatories type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Fetch.stakeholders type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Fetch.acting_parties type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Fetch.interface_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Exercise.interface_id type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Rollback.children type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.Node submsg_callback:true
# Create node for display parsing
com.daml.ledger.api.v2.interactive.transaction.v1.cb.CreateDisplay type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.CreateDisplay.argument type:FT_STATIC
com.daml.ledger.api.v2.interactive.transaction.v1.cb.ExerciseDisplay type:FT_POINTER
com.daml.ledger.api.v2.interactive.transaction.v1.cb.ExerciseDisplay.chosen_value type:FT_STATIC
com.daml.ledger.api.v2.interactive.transaction.v1.cb.NodeDisplay submsg_callback:true
EOF

echo "untyped_versioned_message.options file..."
cat > untyped_versioned_message.options << 'EOF'
* anonymous_oneof:true
com.digitalasset.canton.version.v1.UntypedVersionedMessage.data type:FT_POINTER
EOF

echo "topology.options file..."
cat > topology.options << 'EOF'
# * anonymous_oneof:true

com.digitalasset.canton.protocol.v30.NamespaceDelegation.namespace type:FT_POINTER

com.digitalasset.canton.protocol.v30.DecentralizedNamespaceDefinition.decentralized_namespace type:FT_POINTER
com.digitalasset.canton.protocol.v30.DecentralizedNamespaceDefinition.owners type:FT_POINTER
com.digitalasset.canton.protocol.v30.DecentralizedNamespaceDefinition.owners max_count:8

com.digitalasset.canton.protocol.v30.OwnerToKeyMapping.member type:FT_POINTER
com.digitalasset.canton.protocol.v30.OwnerToKeyMapping.public_keys type:FT_POINTER
com.digitalasset.canton.protocol.v30.OwnerToKeyMapping.public_keys max_count:8

com.digitalasset.canton.protocol.v30.SynchronizerTrustCertificate.participant_uid type:FT_POINTER
com.digitalasset.canton.protocol.v30.SynchronizerTrustCertificate.synchronizer_id type:FT_POINTER

com.digitalasset.canton.protocol.v30.ParticipantSynchronizerPermission.synchronizer_id type:FT_POINTER
com.digitalasset.canton.protocol.v30.ParticipantSynchronizerPermission.participant_uid type:FT_POINTER

com.digitalasset.canton.protocol.v30.PartyHostingLimits.synchronizer_id type:FT_POINTER
com.digitalasset.canton.protocol.v30.PartyHostingLimits.party type:FT_POINTER

com.digitalasset.canton.protocol.v30.VettedPackages.participant_uid type:FT_POINTER
com.digitalasset.canton.protocol.v30.VettedPackages.package_ids type:FT_POINTER
com.digitalasset.canton.protocol.v30.VettedPackages.package_ids max_count:8
com.digitalasset.canton.protocol.v30.VettedPackages.VettedPackage.package_id type:FT_POINTER

com.digitalasset.canton.protocol.v30.PartyToParticipant.party type:FT_POINTER
com.digitalasset.canton.protocol.v30.PartyToParticipant.participants type:FT_POINTER
com.digitalasset.canton.protocol.v30.PartyToParticipant.participants max_count:8
com.digitalasset.canton.protocol.v30.PartyToParticipant.HostingParticipant.participant_uid type:FT_POINTER

com.digitalasset.canton.protocol.v30.SynchronizerParametersState.synchronizer_id type:FT_POINTER

com.digitalasset.canton.protocol.v30.MediatorSynchronizerState.synchronizer_id type:FT_POINTER
com.digitalasset.canton.protocol.v30.MediatorSynchronizerState.active type:FT_POINTER
com.digitalasset.canton.protocol.v30.MediatorSynchronizerState.active max_count:8
com.digitalasset.canton.protocol.v30.MediatorSynchronizerState.observers type:FT_POINTER
com.digitalasset.canton.protocol.v30.MediatorSynchronizerState.observers max_count:8

com.digitalasset.canton.protocol.v30.SequencerSynchronizerState.synchronizer_id type:FT_POINTER
com.digitalasset.canton.protocol.v30.SequencerSynchronizerState.active type:FT_POINTER
com.digitalasset.canton.protocol.v30.SequencerSynchronizerState.active max_count:8
com.digitalasset.canton.protocol.v30.SequencerSynchronizerState.observers type:FT_POINTER
com.digitalasset.canton.protocol.v30.SequencerSynchronizerState.observers max_count:8

com.digitalasset.canton.protocol.v30.PartyToKeyMapping.party type:FT_POINTER
com.digitalasset.canton.protocol.v30.PartyToKeyMapping.signing_keys type:FT_POINTER
com.digitalasset.canton.protocol.v30.PartyToKeyMapping.signing_keys max_count:8

com.digitalasset.canton.protocol.v30.SynchronizerUpgradeAnnouncement.successor_physical_synchronizer_id type:FT_POINTER

com.digitalasset.canton.protocol.v30.SequencerConnectionSuccessor.sequencer_id type:FT_POINTER
com.digitalasset.canton.protocol.v30.SequencerConnectionSuccessor.synchronizer_id type:FT_POINTER
com.digitalasset.canton.protocol.v30.SequencerConnectionSuccessor.SequencerConnection.Grpc.endpoints type:FT_POINTER
com.digitalasset.canton.protocol.v30.SequencerConnectionSuccessor.SequencerConnection.Grpc.endpoints max_count:8
com.digitalasset.canton.protocol.v30.SequencerConnectionSuccessor.SequencerConnection.Grpc.custom_trust_certificates type:FT_POINTER

com.digitalasset.canton.protocol.v30.DynamicSequencingParametersState.synchronizer_id type:FT_POINTER

com.digitalasset.canton.protocol.v30.SignedTopologyTransaction.transaction type:FT_POINTER

EOF


# Generate nanopb C/H code for protobuf messages
generate_nanopb_code() {
  local include_paths=$1
  local proto_file=$2
  local extra_opts=${3:-""}

  yellow "Generating nanopb C/H code for $proto_file"

  # Extract the base name for the options file
  local base_name=$(basename "$proto_file" .proto)
  local options_file="${base_name}.options"

  # Build the protoc command
  local protoc_cmd="$PROTOC --nanopb_out=$OUTPUT_DIR"

  # Add options file if it exists
  if [ -f "$options_file" ]; then
    protoc_cmd="$protoc_cmd --nanopb_opt=-f$options_file"
  fi

  # Add common options to handle recursion and static allocation
  protoc_cmd="$protoc_cmd --nanopb_opt=-T"
  protoc_cmd="$protoc_cmd --nanopb_opt=-s\"max_size:1024\""

  # Add extra options if provided
  if [ -n "$extra_opts" ]; then
    protoc_cmd="$protoc_cmd $extra_opts"
  fi

  # Add include paths
  protoc_cmd="$protoc_cmd -I$include_paths -I. --plugin=protoc-gen-nanopb=$NANOPB_GENERATOR $proto_file"
  py_protoc_cmd="$PROTOC -I$include_paths -I. --python_out=$OUTPUT_DIR --pyi_out=$OUTPUT_DIR $proto_file"

  # Execute the command
  yellow "Running: $protoc_cmd"
  eval $protoc_cmd
  eval $py_protoc_cmd
}

# Generate nanopb C/H code
echo "Generating nanopb C/H code from protobuf definitions..."

# Replace "bool" and "enum" field names in value.proto to make sure only C allowed names are used.
sed -i -E 's/\bbool bool\b/bool bool_/g; s/\bEnum enum\b/Enum enum_/g' com/daml/ledger/api/v2/value.proto
sed -i -E 's/\bbool bool\b/bool bool_/g; s/\bEnum enum\b/Enum enum_/g' com/daml/ledger/api/v2/value_cb.proto

# Remove scalapb annotations from all proto files to avoid nanopb generation issues (we don't use them anyway as we are in C)
find "$PROTOCOL_PROTO_PATH" "$CRYPTO_PROTO_PATH" -name "*.proto" \
  -exec sed -i -E '/option \(scalapb\.message\)|import "scalapb\/scalapb.proto";/d' {} +

# Generate value.proto first to ensure all dependencies are available
generate_nanopb_code "." "com/daml/ledger/api/v2/value.proto"
generate_nanopb_code "." "com/daml/ledger/api/v2/value_cb.proto"

# Generate essential proto files
generate_nanopb_code "." "google/protobuf/empty.proto"
generate_nanopb_code "." "google/rpc/status.proto"
generate_nanopb_code "." "google/rpc/error_details.proto"
generate_nanopb_code "." "google/protobuf/any.proto"
generate_nanopb_code "." "google/protobuf/duration.proto"
generate_nanopb_code "." "google/protobuf/timestamp.proto"

# Generate main interactive submission service
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/interactive/interactive_submission_service.proto"
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/interactive/device.proto"
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/interactive/interactive_submission_common_data.proto"
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/interactive/transaction/v1/interactive_submission_data.proto"
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/interactive/transaction/v1/interactive_submission_data_cb.proto"

# Generate topology files
generate_nanopb_code "$COMMUNITY_PROTO_PATH" "$COMMUNITY_CANTON_PROTO_PATH/version/v1/untyped_versioned_message.proto"
generate_nanopb_code "$COMMUNITY_PROTO_PATH" "$PROTOCOL_PROTO_PATH/traffic_control_parameters.proto"
generate_nanopb_code "$COMMUNITY_PROTO_PATH" "$PROTOCOL_PROTO_PATH/synchronizer_parameters.proto"
generate_nanopb_code "$COMMUNITY_PROTO_PATH" "$PROTOCOL_PROTO_PATH/sequencing_parameters.proto"
generate_nanopb_code "$COMMUNITY_PROTO_PATH" "$PROTOCOL_PROTO_PATH/topology.proto"
generate_nanopb_code "$COMMUNITY_PROTO_PATH" "$CRYPTO_PROTO_PATH/crypto.proto"

# Generate other ledger API files
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/offset_checkpoint.proto"
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/package_reference.proto"
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/trace_context.proto"
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/commands.proto"
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/completion.proto"
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/event.proto"
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/transaction.proto"
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/transaction_filter.proto"
generate_nanopb_code "$LEDGER_API_PROTO_PATH" "$LEDGER_API_V2_PATH/crypto.proto"


green "Done! Generated files are in: $OUTPUT_DIR"
