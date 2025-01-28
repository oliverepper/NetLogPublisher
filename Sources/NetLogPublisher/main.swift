import msg_server
import Combine
import Foundation

public final class LogServer {
    public class LogPublisher: Combine.Publisher {
        public typealias Output = String
        public typealias Failure = Never

        private let logServer: LogServer

        init(logServer: LogServer) {
            self.logServer = logServer
        }

        public func receive<S>(subscriber: S)
          where S : Subscriber,
                S.Failure == Failure,
                S.Input == Output {
            let subscription = Subscription(subscriber: subscriber, logServer: logServer)
            subscriber.receive(subscription: subscription)
        }

        private class Subscription<Subscriber: Combine.Subscriber>: Combine.Subscription
          where Subscriber.Input == String,
                Subscriber.Failure == Never {
            private let logServer: LogServer
            private let cancellable: AnyCancellable

            init(subscriber: Subscriber, logServer: LogServer) {
                self.logServer = logServer
                self.cancellable = logServer._relay
                  .receive(on: RunLoop.main)
                  .sink { msg in
                      _ = subscriber.receive(msg)
                  }
            }

            func request(_ demand: Subscribers.Demand) {
                // no-op
            }

            func cancel() {
                cancellable.cancel()
            }
        }
    }

    private let _relay = PassthroughSubject<String, Never>()
    public var messages: LogPublisher {
        .init(logServer: self)
    }

    init(serviceName: String = "0") {
        let server = msg_server_create(serviceName)
        msg_server_add_callback(
          server,
          { msg, ctx in
              guard let msg, let ctx else { return }

              let this = Unmanaged<LogServer>.fromOpaque(ctx).takeUnretainedValue()
              this._relay.send(.init(cString: msg))

          }, Unmanaged<LogServer>.passUnretained(self).toOpaque())
    }
}

var error = msg_server_error_t()
var status = msg_server_init(&error)
if status != MSG_SERVER_SUCCESS {
    if let errorMessage = error.message {
        print(errorMessage)
        free(errorMessage)
    }
} else {
    print("Init successfull")
}

var cancellables = Set<AnyCancellable>()

struct Person: CustomDebugStringConvertible {
    let name: String

    var debugDescription: String {
        "Person <name: \(name)>"
    }
}

LogServer(serviceName: "1979").messages
  .sink { message in
      print("Simon says: \(message)")
  }.store(in: &cancellables)

LogServer(serviceName: "1980").messages
  .map {Person(name: $0.trimmingCharacters(in: .newlines)) }
  .sink { person in
      print(person.debugDescription)
  }.store(in: &cancellables)


RunLoop.current.run()
