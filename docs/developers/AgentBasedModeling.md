# Agent-based modeling

The agent mechanism lives in `source/plugins/data/Agents`. An `AgentNetwork` is a
`DirectedGraphNetwork` that a `ModalModel` activates. Each entity that reaches the
`ModalModel` runs one synchronous round of the agents, so the `Create` that feeds it sets when
the rounds happen and how many there are. The simulation clock, the replications, the
persistence and the statistics are the ones of GenESyS.

## Classes

| Class | Role |
|---|---|
| `AgentNetwork` | Runs the rounds, checks the model and applies the failure policy. Owns the optional environment. |
| `AgentNode` | One agent. References the behavior of its type and owns its world model. |
| `AgentChannel` | Directed link between two agents, with the message types it accepts. |
| `MessageBroker` | Keeps the messages sent in a round and delivers them in the next one. |
| `AgentBehavior` | Extension point: how a type of agent decides and how a decision changes its world model. |
| `AgentWorldModel` | Extension point: what one agent is and knows. |
| `AgentEnvironment` | Extension point: what every agent perceives of the shared environment and how messages change it. |
| `AgentMessage` | Extension point: a message to another agent or, with no receiver, to the environment. |
| `AgentDecision` | The decision of one agent in one round, with the messages it sends. Subclassed when `apply` needs more. |

Two specializations come with the mechanism. `RuleBehavior`, `RuleWorldModel`, `RuleDecision`
and `RuleMessage` evaluate rules on GenESyS Variables. `LLMBehavior`, `LLMWorldModel`,
`LLMDecision`, `TextMessage` and `PublicConsultationEnvironment` consult an LLM through an
`AISupport`.

## A round

1. Perceive. For each agent, sorted by name, the network gathers the messages the broker has for
   it, what it perceives of the environment and the agents its outgoing channels reach.
2. Decide. The network calls `decide(perception, worldModel)`, which returns an `AgentDecision`
   and must not change any state. Each message of the decision must have a channel from the
   sender to the receiver that accepts its type or, with no receiver, an environment that accepts
   it. An exception thrown by `decide` or an invalid message fails the decision.
3. Apply. Failed decisions are counted. When the failure limit is reached the round is cancelled
   and the simulation stops. Otherwise the network calls `apply(decision, worldModel)` for every
   valid decision, gives the messages to the broker and to the environment, and redelivers the
   messages of the agents whose decision failed.
4. Record. Each behavior gets `endRound(agents)` to record its measures, the network counts
   decisions, messages and failures, and the trace receives, at level `L5_event`, one line per
   agent with the replication, time, round, decision, messages and world model.

Since every agent decides before any decision is applied, a message is perceived only in the next
round, and what an agent perceives does not depend on the order of the agents.

## Creating a new type of agent

Write subclasses of the extension points and register them as plugins.

- `AgentWorldModel`: keep the initial configuration in fields saved by `_saveInstance` and loaded
  by `_loadInstance`, and the execution state in fields that `reset()` sets back to the initial
  configuration at each replication. `show()` is what the trace prints. The world model is saved
  inside the record of its agent, so it is created with `insertIntoModel` false by the base class.
- `AgentBehavior`: implement `decide`, `apply`, `worldModelType`, `producedMessageTypes` and
  `acceptedMessageTypes`. The model check refuses an agent whose world model is not of the type
  the behavior names, a channel between agents with no message type in common and an agent that
  produces a type that neither the environment nor its channels accept. Override `endRound` and
  `_createInternalStatisticReporters` to create counters and collectors. A behavior is shared by
  all the agents of its type, so any per-agent state belongs to the world model.
- `AgentDecision`: add the fields `apply` needs beyond the messages, such as the values computed
  by the rules. `show()` is what the trace prints.
- `AgentMessage`: give it a type name, usually `Util::TypeOf<YourMessage>()`, and a `show()`.
- `AgentEnvironment` (optional): implement `perceive(agent)`, `apply(messages)`, which receives
  the messages of a round ordered by sender name and resolves conflicts between them, and
  `acceptedMessageTypes`. Clear the state in `_initBetweenReplications`. The environment is saved
  inside the record of its network.

Each class that is a model element (behavior, world model, environment) needs the usual
`GetPluginInformation`, `LoadInstance` and `NewInstance`, category `Agents`, and an entry in
`source/plugins/PluginConnectorDummyImpl1.cpp`, since GenESyS only saves and loads registered
types. `LoadInstance` must return an object for an empty record, because the simulator probes
every plugin with one. It returns `nullptr` only to wait for another element the record names,
as `AgentNode` waits for its behavior and `LLMBehavior` for its `AISupport`. Properties shown in
the property editor are `SimulationControl` objects created in the constructor.

`RuleBehavior` is the shortest complete example of properties, persistence, check and
statistics.

## Building a model with agents

```cpp
auto* network = new AgentNetwork(model, "Network");
auto* behavior = new RuleBehavior(model, "Credulous");
behavior->setRules("informed = informed or (received > 0 and UNIF(0, 1) < p)");
behavior->setCondition("informed == 1");
behavior->setPerceptionVariable("received");
behavior->setMeasuredVariables("informed");

auto* agent = new AgentNode(model, "Agent000");
agent->setBehavior(behavior);
agent->setWorldModel(new RuleWorldModel(model));   // owned by the agent
network->addNode(agent);

auto* channel = new AgentChannel(model, "Agent000_Agent001");
channel->setSource(agent);
channel->setDestination(other);
channel->setAcceptedMessageTypes({Util::TypeOf<RuleMessage>()});
network->addEdge(channel);

network->setEnvironment(environment);   // optional, owned by the network
network->setFailureLimit(0);            // 0 never stops; otherwise stop after N failures
network->setFailureLimitPerReplication(true);

auto* create = new Create(model, "Rounds");      // one round per entity
create->setTimeBetweenCreationsExpression("1", Util::TimeUnit::second);
create->setMaxCreations(rounds);
auto* modal = new ModalModel(model, "Agents");
modal->setNetwork(network);
create->connectTo(modal);
modal->connectTo(new Dispose(model, "Done"));
```

Rule-based agents use Variables declared in the model, here `informed`, `received` and `p`. The
initial value of a Variable holds for every agent unless the `RuleWorldModel` of an agent replaces
it with `setInitialValues("informed = 1")`.

LLM agents need an `AISupport` referenced by the `LLMBehavior` and an `LLMWorldModel` with the
persona of each agent. For tests without an LLM, set the provider of the `AISupport` to
`Simulated` and write its `SimulatedResponses` one per line as `agent: response`. The n-th
decision of an agent receives its n-th response, an agent with no response left does not act,
and the count starts again at each replication. A response is a JSON object whose `action` is
`publish` with a `text`, `send` with a `to` and a `text`, or `none`.

## Examples and tests

- `source/applications/modelSpecific/smarts/Agents/Smart_InformationDissemination.cpp`, preset
  `terminal-smart-information-dissemination`: credulous and skeptical rule-based agents on a
  ring, a random network or a small world.
- `source/applications/modelSpecific/smarts/Agents/Smart_PublicConsultation.cpp`, preset
  `terminal-smart-public-consultation`: three LLM agents on a public consultation, in a
  controlled mode with the simulated provider or a demonstration mode with an LLM provider.
- `source/tests/unit/test_agents.cpp`, target `genesys_test_agents`, label `abm`.

```sh
cmake --preset terminal-smart-information-dissemination
cmake --build --preset terminal-smart-information-dissemination
./build/terminal-smart-information-dissemination/source/applications/modelSpecific/genesys_terminal_application --topology=smallworld --agents=40 --skeptics=0.25 --rounds=20
```
