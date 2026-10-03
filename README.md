# PS4-TrophyProbe

Prototype de diagnóstico **offline/local** para estudar o fluxo de inicialização do subsistema de troféus no PS4.

## Objetivo da V0.1

A primeira versão não desbloqueia troféus. Ela executa, uma etapa por vez, o mesmo caminho básico documentado no sample de troféus do OpenOrbis e registra o retorno de cada chamada:

1. `sceUserServiceInitialize`
2. `sceUserServiceGetInitialUser`
3. `sceSysmoduleLoadModule(ORBIS_SYSMODULE_NP_TROPHY)`
4. `sceNpTrophyCreateContext`
5. `sceNpTrophyCreateHandle`
6. `sceNpTrophyRegisterContext`

O resultado é mostrado por notificações do PS4 e também tentamos gravar um log em `/data/PS4-TrophyProbe.log`.

A intenção é descobrir **em qual etapa o contexto deixa de ser válido**, antes de adicionar qualquer tentativa de unlock ou experimentos com títulos não instalados.

## Importante

- V0.1 **não chama** `sceNpTrophyUnlockTrophy`.
- Não usa PSN e não depende de login online.
- O protótipo foi escrito para o **OpenOrbis PS4 Toolchain**.
- A estrutura de empacotamento PKG (ícone, SFO e assets) será adicionada depois que confirmarmos que o binário de diagnóstico compila e inicia corretamente.

## Compilar

Com `OO_PS4_TOOLCHAIN` apontando para a instalação do OpenOrbis:

```bash
make
```

O alvo atual gera `eboot.bin`.

## Próximo teste

Depois de rodar no PS4, copie ou fotografe os códigos mostrados para estas etapas:

```text
USER_SERVICE_INIT
GET_INITIAL_USER
LOAD_NP_TROPHY
CREATE_CONTEXT
CREATE_HANDLE
REGISTER_CONTEXT
```

Esses códigos serão a base da V0.2.

## Referência técnica

A sequência básica usada aqui segue o sample público de troféus do OpenOrbis PS4 Toolchain e as assinaturas expostas por `orbis/NpTrophy.h`.
