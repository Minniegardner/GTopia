import fs from "fs";

// @note interfaces for items.json structure
interface ItemData {
  item_id: number;
  name: string;
}

interface ItemsFile {
  version: number;
  item_count: number;
  items: ItemData[];
}

export class GenerateItemsEnum {
  // @note cached regex for sanitization (combines non-alphanumeric removal + space to underscore)
  private static readonly SANITIZE_REGEX = /[^A-Za-z0-9_]+/g;
  private static readonly INVALID_NAMES = new Set(["", "0", "NULL", "NONE", "N"]);
  // @note C++ reserved keywords and common macros that need suffix
  private static readonly RESERVED_KEYWORDS = new Set([
    // C keywords
    "AUTO", "BREAK", "CASE", "CHAR", "CONST", "CONTINUE", "DEFAULT", "DO", "DOUBLE", "ELSE",
    "ENUM", "EXTERN", "FLOAT", "FOR", "GOTO", "IF", "INT", "LONG", "REGISTER", "RETURN",
    "SHORT", "SIGNED", "SIZEOF", "STATIC", "STRUCT", "SWITCH", "TYPEDEF", "UNION", "UNSIGNED",
    "VOID", "VOLATILE", "WHILE",
    // C++ keywords
    "ASM", "BOOL", "CATCH", "CLASS", "CONST_CAST", "DELETE", "DYNAMIC_CAST", "EXPLICIT",
    "EXPORT", "FALSE", "FRIEND", "INLINE", "MUTABLE", "NAMESPACE", "NEW", "OPERATOR",
    "PRIVATE", "PROTECTED", "PUBLIC", "REINTERPRET_CAST", "STATIC_CAST", "TEMPLATE", "THIS",
    "THROW", "TRUE", "TRY", "TYPEID", "TYPENAME", "USING", "VIRTUAL", "WCHAR_T",
    // C++11 and later
    "ALIGNAS", "ALIGNOF", "CHAR16_T", "CHAR32_T", "CONSTEXPR", "DECLTYPE", "NOEXCEPT",
    "NULLPTR", "STATIC_ASSERT", "THREAD_LOCAL",
    // C++20
    "CONCEPT", "REQUIRES", "CO_AWAIT", "CO_RETURN", "CO_YIELD",
    // Common macros
    "NULL", "DATE", "TIME"
  ]);

  private itemsData: ItemsFile | null = null;

  private sanitizeEnumBaseName(name: string): string {
    return name.trim().replace(GenerateItemsEnum.SANITIZE_REGEX, "_").toUpperCase();
  }

  // @note loadFromFile - loads items.json from given path, validates file type
  public loadFromFile(filePath: string): void {
    if (!filePath.endsWith(".json")) {
      throw new Error(`Invalid file type. Expected a .json file: ${filePath}`);
    }

    const fileStat = fs.statSync(filePath);
    if (!fileStat.isFile()) {
      throw new Error(`Not a valid file: ${filePath}`);
    }

    this.itemsData = JSON.parse(fs.readFileSync(filePath, "utf-8"));
  }

  get itemsVersion(): number {
    return this.itemsData?.version ?? -1;
  }

  get itemsCount(): number {
    return this.itemsData?.item_count ?? -1;
  }

  // @note buildEnum - generates C++ enum string from loaded items data
  public buildEnum(tabSize: number = 4, prefix: string = ""): string {
    if (!this.itemsData) {
      throw new Error("Items data not loaded. Please load data before building enum.");
    }

    if (prefix.trim()) {
      prefix = prefix.trim().toUpperCase();
      if (!prefix.endsWith("_")) {
        prefix += "_";
      }
    }

    const items = this.itemsData.items;
    const len = items.length;
    const indent = " ".repeat(tabSize);
    const invalidNames = GenerateItemsEnum.INVALID_NAMES;
    const reservedKeywords = GenerateItemsEnum.RESERVED_KEYWORDS;
    const regex = GenerateItemsEnum.SANITIZE_REGEX;

    // @note pre-allocate array for better performance
    const lines: string[] = new Array(len);
    let lineIdx = 0;

    // @note track used enum names to prevent duplicates
    const usedNames = new Set<string>();

    for (let i = 0; i < len; i++) {
      const item = items[i];
      if (!item) continue;

      // @note inline sanitization for hot loop performance
      let sanitized = this.sanitizeEnumBaseName(item.name);

      // @note handle leading digit
      if (sanitized.charCodeAt(0) >= 48 && sanitized.charCodeAt(0) <= 57) {
        sanitized = "_" + sanitized;
      }

      if (invalidNames.has(sanitized)) continue;

      // @note add suffix to reserved keywords to avoid conflicts
      if (reservedKeywords.has(sanitized)) {
        sanitized = `${sanitized}_`;
      }

      // @note handle duplicate enum names by appending _2, _3, etc.
      let finalName = sanitized;
      let suffix = 2;
      while (usedNames.has(finalName)) {
        finalName = `${sanitized}_${suffix}`;
        suffix++;
      }
      usedNames.add(finalName);

      lines[lineIdx++] = `${indent}${prefix}${finalName} = ${item.item_id},`;
    }

    // @note trim unused array slots
    lines.length = lineIdx;

    return `enum eItems {\n${lines.join("\n")}\n};\n`;
  }

  // @note buildItemUtilsEItemIDEnum - output format aligned with Source/Item/ItemUtils.h
  public buildItemUtilsEItemIDEnum(tabSize: number = 4): string {
    if (!this.itemsData) {
      throw new Error("Items data not loaded. Please load data before building enum.");
    }

    const items = this.itemsData.items;
    const len = items.length;
    const indent = " ".repeat(tabSize);
    const invalidNames = GenerateItemsEnum.INVALID_NAMES;
    const reservedKeywords = GenerateItemsEnum.RESERVED_KEYWORDS;

    const lines: string[] = new Array(len);
    let lineIdx = 0;
    const usedNames = new Set<string>();

    for (let i = 0; i < len; i++) {
      const item = items[i];
      if (!item) continue;

      let sanitized = this.sanitizeEnumBaseName(item.name);
      if (invalidNames.has(sanitized)) continue;

      if (reservedKeywords.has(sanitized)) {
        sanitized = `${sanitized}_`;
      }

      // @note Keep ItemUtils-compatible style: ITEM_ID_<NAME>,
      // and resolve duplicates deterministically with the item id.
      let finalName = `ITEM_ID_${sanitized}`;
      if (usedNames.has(finalName)) {
        finalName = `${finalName}_${item.item_id}`;

        let suffix = 2;
        while (usedNames.has(finalName)) {
          finalName = `ITEM_ID_${sanitized}_${item.item_id}_${suffix}`;
          suffix++;
        }
      }

      usedNames.add(finalName);
      lines[lineIdx++] = `${indent}${finalName} = ${item.item_id},`;
    }

    lines.length = lineIdx;
    return `enum eItemID \n{\n${lines.join("\n")}\n};\n`;
  }
}
