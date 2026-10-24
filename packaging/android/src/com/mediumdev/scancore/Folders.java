package com.mediumdev.scancore;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;

// A folder chosen through the system's picker (a content:// address), read
// with one query to its documents provider. Qt's file engine asks once per
// file and attribute, which made a big Mihon downloads folder take minutes.
public final class Folders {
    private Folders() {}

    // One line per entry, "name<TAB>1 for a folder, else 0<TAB>address", or
    // null when the folder cannot be read.
    public static String list(Context context, String address) {
        try {
            final Uri uri = Uri.parse(address);
            final String parent = DocumentsContract.isDocumentUri(context, uri) ? DocumentsContract.getDocumentId(uri)
                                                                                : DocumentsContract.getTreeDocumentId(uri);
            final Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(uri, parent);
            final String[] columns = {DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                                      DocumentsContract.Document.COLUMN_MIME_TYPE};
            final StringBuilder out = new StringBuilder();
            try (Cursor cursor = context.getContentResolver().query(children, columns, null, null, null)) {
                if (cursor == null)
                    return null;
                while (cursor.moveToNext()) {
                    final String name = cursor.getString(1);
                    if (name == null)
                        continue;
                    out.append(name.replace('\t', ' ').replace('\n', ' '))
                        .append('\t')
                        .append(DocumentsContract.Document.MIME_TYPE_DIR.equals(cursor.getString(2)) ? '1' : '0')
                        .append('\t')
                        .append(DocumentsContract.buildDocumentUriUsingTree(uri, cursor.getString(0)).toString())
                        .append('\n');
                }
            }
            return out.toString();
        } catch (Exception e) {
            return null;
        }
    }
}
