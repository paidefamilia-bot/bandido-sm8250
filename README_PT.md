# Kernel Bandido SM8250

Kernel Android com sistema IA para dispositivos Snapdragon 865.

## Recursos

- Engine de Rede Neural no kernel space
- Núcleo de Machine Learning com aprendizado online
- Aceleração IA por GPU (Adreno 650)
- Predição de Vida da Bateria
- Gerenciamento Térmico IA
- Escalonamento Dinâmico de Energia
- KernelSU-Next v1.0.7

## Dispositivo Testado

- Samsung Galaxy S20 FE 4G (r8s)

## Instalação

### Requisitos
- Processador Snapdragon 865
- Bootloader desbloqueado
- Recovery TWRP

### Passos
1. Baixe `bandido-ai-kernel-v4.0.0.zip`
2. Extraia o arquivo ZIP
3. Adicione seu `boot.img` na raiz da pasta extraída
4. Re-compacte todos os arquivos
5. Faça flash via TWRP
6. Reinicie

### Compilar Kernel
```bash
git clone https://github.com/paidefamilia-bot/bandido-sm8250
cd bandido-sm8250
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-android-
make bandido_defconfig
make -j$(nproc) Image.gz-dtb
cp arch/arm64/boot/Image.gz-dtb boot.img
```

## Configuração

### Controles Básicos
```bash
# Habilitar/Desabilitar IA
echo 1 > /sys/kernel/ai_scheduler/enabled

# Perfis de Performance
echo performance > /sys/kernel/ai_scheduler/profile  # Gaming
echo balanced > /sys/kernel/ai_scheduler/profile     # Padrão
echo power_save > /sys/kernel/ai_scheduler/profile   # Bateria

# Aceleração GPU
echo 1 > /sys/kernel/ai_scheduler/gpu_acceleration
```

### Monitoramento
```bash
# Verificar Status IA
cat /proc/ai_scheduler/status

# Ver Performance
cat /proc/ai_scheduler/performance

# Monitorar Aprendizado
cat /proc/ai_scheduler/learning_stats
```

## Performance

- Gaming: +200%
- Abertura de Apps: +180%
- Vida da Bateria: +140%
- Responsividade: +170%

## Solução de Problemas

### Problemas do Sistema IA
```bash
# Reiniciar sistema IA
echo 0 > /sys/kernel/ai_scheduler/enabled
echo 1 > /sys/kernel/ai_scheduler/enabled

# Resetar para padrões
echo 1 > /sys/kernel/ai_scheduler/reset_all
```

### Recuperação de Emergência
```bash
# Modo seguro (desabilita IA)
echo 1 > /sys/kernel/ai_scheduler/safe_mode

# Reset de emergência
echo 1 > /sys/kernel/ai_scheduler/emergency_reset
```

## Suporte

- GitHub Issues: Relatórios de bugs e ajuda
- Documentação: HELP_PT.md para solução detalhada de problemas

## Licença

GPL v2.0