## Prompts 

### Estilingue em Primeira Pessoa
- **Prompt**: Quando o personagem está em primeira pessoa, faça com que ele segurar o botão esquerdo do mouse carregue um estilingue o o libere ao soltar. Isso deve enviar um projétil (esfera marrom) em linha reta no centro da visão do personagem. Prepare uma colisão para este projétil ser integrado com a colisão de inimigos no futuro. No futuro também haverá a animação de um estilingue sendo puxado enquanto carrega, então deixe esta parte do código encaminhada para fácil atualização. 

Por enquanto, faça que o projeto suma ao colidir com uma parede, mas considere que no futuro ele tera colisões especiais com outros inimigos e objetos.
- **Arquivos afetados**:
  - `src/main.cpp`
  - `src/shader_fragment.glsl`
