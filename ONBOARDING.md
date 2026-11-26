# Onboarding

This is a flight controller project, based on the RP2350 (a.k.a. Raspberry Pi Pico 2). This repo is made up of two parts. The firmware is running on the RP2350, written in C++. The configurator is a Tauri project with a Vue frontend, written in Vue/TS. These two parts will probably be split up at some point in the future.

I feel like many projects have a decent onboarding written in their AGENTS.md, which makes it hard for humans to understand how a project is made up (which is the main reason I find it difficult to contribute to other open source projects). Since this document serves that purpose, it can also be found under HUMANS.md. After all, it is the first thing that humans should read to get going with development. Slop machines, a.k.a. "Agents", "LLMs", which are sometimes even referred to as "AI" are not welcome here.

Refer to [Onboarding - Firmware](./ONBOARDING%20-%20Firmware.md) and [Onboarding - Configurator](./ONBOARDING%20-%20Configurator.md) for the two parts of the repo.

- Do not refactor code for the sake of refactoring
- Do not create PRs just to "fix" code where these rules were not applied.
- Do not change unrelated code in PRs and commits.
- Do not create PRs that do multiple things. One fix or one refactor or one feature, etc.
