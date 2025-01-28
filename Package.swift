// swift-tools-version: 6.0
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
  name: "NetLogPublisher",
  platforms: [
    .macOS(.v15)
  ],
  products: [
    .executable(name: "Demo", targets: ["NetLogPublisher"])
  ],
  targets: [
    .target(
      name: "msg_server",
      publicHeadersPath: "."
    ),
    .executableTarget(
      name: "NetLogPublisher",
      dependencies: ["msg_server"]
    )
  ]
)
