## Prompts 

### Adicao da Fada Orbitando o Personagem
- **Prompt**: No jogo original, o modelo de uma fada fica circulando em torno do personagem princiapl. Crie um objeto esférico (como modelo provisório) que seguira uma curva de bezier ao redor do personagem principal, animado para ficar voando ao redor dele com o passar do tempo. Siga as diretrizes e convenções usadas no código atualmente. 
A curva de bezier deve descrever uma trajetória circular acima da cabeça do personagem com uma ondulação vertical (ir levemente para cima e para baixo)
- **Arquivos afetados**:
  - `src/main.cpp`
  - `src/shader_fragment.glsl`

### Relatorio das Alteracoes e Ajustes de Distancia/Amplitude
- **Prompt sintetizado**: I want the fairy to slightly move up and down, like if lightly flying/floating in place while it circles. Im having truble precisely tuning the parameters to make the movement precise. Make a clear set of params that define:
- number of oscilacions per circle
- speed of circling
- amplitude oscilations (how high or low it flies)
- **Arquivos afetados**:
  - `src/main.cpp`