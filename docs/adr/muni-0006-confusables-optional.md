# muni-0006. Confusable detection as an optional component

Status: Accepted

## Context

Names that players and users see (usernames, chat) can impersonate
others with look-alike characters. UTS #39 defines confusable
skeletons and restriction levels for this; they depend on
normalization and Script_Extensions.

## Decision

UTS #39 confusable skeletons and restriction levels are in scope before
1.0, as an optional component built after normalization. A build that
leaves it out pays nothing for it.

## Consequences

Consumers get impersonation checks from the same Unicode version as
everything else.
