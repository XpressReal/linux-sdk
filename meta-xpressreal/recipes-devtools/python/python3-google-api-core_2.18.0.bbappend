# Remove grpcio runtime dependency since grpcio is not available in the current layers.
# The core google-api-core can function with HTTP transport only; gRPC transport
# support (via grpcio extension) is excluded here. If gRPC is needed in the future,
# a recipe for python3-grpcio must be added first.
RDEPENDS:${PN}:remove = "python3-grpcio"
