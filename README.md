# Computação Gráfica e Visualização I (INF01047) - INF/UFRGS

## Integrantes da dupla

- **Aluno 1 - Nome**: Eduardo Magnus Lazuta
- **Aluno 1 - Cartão UFRGS**: 325091

- **Aluno 2 - Nome**: Vitor Alexandre Arguilar
- **Aluno 2 - Cartão UFRGS**: 344617

## Aplicação desenvolvida

- **Título do trabalho**: Réplica do The Legend of Zelda Ocarina of Time (versão do nintendo 3DS)
- **O que foi implementado**: A nossa implementação foi uma recriação das salas iniciais da seção "Inside the Great Deku Tree", do jogo The Legend of Zelda Ocarina of Time.
## Contribuições
### Eduardo
- Implementação e integração dos modelos de cenas na aplicação.
- Integração do modelo e das animações do Link.
- Implementação Inicial das cameras em primeira e terceira pessoa.
- Implementação da colisão.
- Refinamento da movimentação do personagem.
- Sistemas de portas e escalada. 
- Adição do baú e da escada presa na teia
### Vitor
- Refinamentos na movimentação do personagem
- Inclusão da fada e sua animação
- Inclusão do tiro de estilingue
- Inclusão das animações de explosão do estilingue e morte dos inimigos.
- Implementação de todos os inimigos.
- Camera Lock-on

## Uso de IA
Agentes de IA foram bastante utilizados no desenvolvimento do trabalho, em especial Codex, Claude code. Para várias das funcionalidades eles foram usados como um criador de código inicial, a partir do qual refinamos os resultados manualmente ou com prompts adicionais, visto que as ferramentas muitas vezes não dão resultados adequados imediatamente. 
## Imagens de Ilustração

## Manual
### Atalhos
- W/S - Movem o personagem para frente/trás. Enquanto escalando, move para cima/baixo.
- A/D - Gira o personagem para a esquerda/direita enquanto em terceira pessoa. Faz um strafe para a mesma direção em volta no inimigo enquanto na camera lock-on. Move horizontalmente enquanto escalando vinhas.
- Shift - Corre enquanto estiver no chão.
- Space - Pula enquanto estiver no chão.
- E - Ataque com a espada.
- Q - Defende com o escudo.
- 1..9 - Seleciona animação do ataque
- Enter - Abre baús e portas próximas
- O - Inicia ou termina a escalada de escadas ou vinhas.
- Tab - Ativa o estilingue, desabilitando a espada/escudo.
- Mouse Left - Atira com o estilingue enquanto em primeira pessoa ou com slingshot equipado.
- Movimento mouse - Controle da camera em primeira pessoa.
- P - Ativa ou desativa visão em primeira pessoa.
- K - Ativa ou desativa camera lock-on.
- Seta direita/esquerda: Altera qual inimigo a camera lock-on trava.
- Esc - Fecha o jogo

### Interações
* Vinhas: Podem ser escaladas verticalmente e horizontalmente.
* Escadas: Podem ser escaladas verticalmente
* Portas e baús: Podem ser abertas.
* Teias: Algumas podem ser quebradas com uma queda longa.
* Escada presa: Pode ser atingida com o estilingue para cair no chão.

### Inimigos
* Deku Baba (planta): Inimigo simples, bate no personagem corpo-a-corpo case ele se aproxime.
* Skullwalltula (aranha da parede): Fica nas vinhas, subindo e descendo e bloqueando a passagem do jogador.
* Skulltula (aranha grande): Bate no personagem corpo-a-corpo case ele se aproxime, mas fica parada por um tempo após atacar. Ela é invulnerável a dano pela frente, mas se o Link bloquear seu ataque, além de ficar atordoada ela se vira, expondo seu ponto fraco.
* Deku Scrub (arbusto): Atira projéteis no jogador quando ele se aproxima. O jogador pode bloquear os projéteis com seu escudo para refletí-los.
## Compilação e Execução
