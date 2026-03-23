# Como hospedar a página no GitHub Pages

## Passo a passo

### 1. Crie um repositório chamado exatamente:
```
ailtonmartins.github.io
```
> Substitua `ailtonmartins` pelo seu usuário real do GitHub.

### 2. Suba o arquivo index.html para esse repositório:
```bash
git clone https://github.com/ailtonmartins/ailtonmartins.github.io
cd ailtonmartins.github.io
cp /caminho/para/index.html .
git add index.html
git commit -m "Add Rutlix project page"
git push
```

### 3. Ative o GitHub Pages:
- Vá em **Settings → Pages** no repositório
- Source: **Deploy from a branch**
- Branch: **main** / **(root)**
- Salve

### 4. Acesse em:
```
https://ailtonmartins.github.io
```

---

## Alternativa: hospedar dentro do repositório do Rutlix

Coloque o `index.html` numa pasta `docs/` dentro do repo `rutlix`:

```bash
mkdir docs
cp index.html docs/
git add docs/
git commit -m "Add GitHub Pages"
git push
```

Em **Settings → Pages**, escolha:
- Branch: `main`
- Folder: `/docs`

A URL será:
```
https://ailtonmartins.github.io/rutlix
```

---

## Estrutura de Releases sugerida

Para que os links de download da página funcionem, crie releases no GitHub
com estes nomes de arquivo:

| Arquivo | Formato |
|---|---|
| `rutlix_2.1_amd64.deb` | Debian/Ubuntu |
| `Rutlix_Record-2.1-x86_64.AppImage` | AppImage |
| `rutlix-2.1-1.fc40.x86_64.rpm` | Fedora |

Use `gh release create v2.1 *.deb *.AppImage *.rpm` (GitHub CLI).
